#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <vector>

namespace dualdsp
{
constexpr int kChannels = 2;
constexpr int kBands = 6;
constexpr double kMaxDelayMs = 500.0;
constexpr double kPi = 3.1415926535897932384626433832795;

struct EqSettings
{
    std::atomic<double> frequencyHz { 1000.0 };
    std::atomic<double> gainDb { 0.0 };
    std::atomic<double> q { 1.0 };
};

struct CutoffSettings
{
    std::atomic<bool> enabled { false };
    std::atomic<double> frequencyHz { 80.0 };
    std::atomic<int> slopeDbPerOct { 12 }; // 12 or 24 dB/oct Butterworth
};

struct ChannelSettings
{
    std::atomic<int> inputRoute { -1 };   // Physical, zero-based input port; -1 = unassigned.
    std::atomic<int> outputRoute { -1 };  // Physical, zero-based output port; -1 = unassigned.
    std::atomic<double> levelDb { 0.0 };
    std::atomic<double> delayMs { 0.0 };
    std::atomic<bool> invertPolarity { false };
    std::atomic<bool> outputMuted { false }; // Separate from level; true means silent output.
    std::array<EqSettings, kBands> eq;
    CutoffSettings highPass, lowPass;

    ChannelSettings()
    {
        highPass.frequencyHz.store(80.0);
        lowPass.frequencyHz.store(16000.0);
        constexpr std::array<double, kBands> defaults { 80, 200, 500, 1200, 4000, 10000 };
        for (int i = 0; i < kBands; ++i)
            eq[static_cast<std::size_t>(i)].frequencyHz.store(defaults[static_cast<std::size_t>(i)]);
    }
};

inline double dbToGain (double db) noexcept { return std::pow(10.0, db / 20.0); }

class ParameterRamp
{
public:
    void prepare (double sampleRate, double rampSeconds, double startingValue)
    {
        duration = std::max(1, static_cast<int>(std::round(sampleRate * rampSeconds)));
        current = target = startingValue;
        increment = 0.0;
        remaining = 0;
    }

    void setTarget (double value) noexcept
    {
        if (value == target) return;
        target = value;
        increment = (target - current) / static_cast<double>(duration);
        remaining = duration;
    }

    double next() noexcept
    {
        if (remaining > 0)
        {
            current += increment;
            if (--remaining == 0) current = target;
        }
        return current;
    }

private:
    int duration = 1, remaining = 0;
    double current = 0.0, target = 0.0, increment = 0.0;
};

class PeakBiquad
{
public:
    void setPeak (double sampleRate, double frequencyHz, double gainDb, double q) noexcept
    {
        const double frequency = std::clamp(frequencyHz, 10.0, sampleRate * 0.48);
        const double quality = std::clamp(q, 0.1, 20.0);
        const double amplitude = std::pow(10.0, gainDb / 40.0);
        const double omega = 2.0 * kPi * frequency / sampleRate;
        const double c = std::cos(omega);
        const double alpha = std::sin(omega) / (2.0 * quality);
        const double a0 = 1.0 + alpha / amplitude;

        b0 = (1.0 + alpha * amplitude) / a0;
        b1 = (-2.0 * c) / a0;
        b2 = (1.0 - alpha * amplitude) / a0;
        a1 = (-2.0 * c) / a0;
        a2 = (1.0 - alpha / amplitude) / a0;
    }

    void setLowPass (double sr, double hz, double q) noexcept
    {
        setCutoff(sr, hz, q, false);
    }

    void setHighPass (double sr, double hz, double q) noexcept
    {
        setCutoff(sr, hz, q, true);
    }

    // Magnitude response of this biquad, evaluated on the unit circle.
    double magnitudeAt (double sr, double hz) const noexcept
    {
        const double omega = 2.0 * kPi * std::clamp(hz, 0.0, sr * 0.5) / sr;
        const double c1 = std::cos(omega), s1 = std::sin(omega);
        const double c2 = std::cos(omega * 2.0), s2 = std::sin(omega * 2.0);
        const double nr = b0 + b1 * c1 + b2 * c2;
        const double ni = -b1 * s1 - b2 * s2;
        const double dr = 1.0 + a1 * c1 + a2 * c2;
        const double di = -a1 * s1 - a2 * s2;
        return std::sqrt((nr * nr + ni * ni) / std::max(1.0e-30, dr * dr + di * di));
    }

    float process (float input) noexcept
    {
        const double x = static_cast<double>(input);
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        // Avoid lingering denormals in the audio thread.
        if (std::abs(z1) < 1.0e-20) z1 = 0.0;
        if (std::abs(z2) < 1.0e-20) z2 = 0.0;
        return static_cast<float>(y);
    }

    void reset() noexcept { z1 = z2 = 0.0; }

private:
    void setCutoff (double sr, double hz, double q, bool highPass) noexcept
    {
        const double f = std::clamp(hz, 10.0, sr * 0.48);
        const double omega = 2.0 * kPi * f / sr;
        const double c = std::cos(omega);
        const double alpha = std::sin(omega) / (2.0 * std::max(0.1, q));
        const double a = 1.0 + alpha;
        const double first = (1.0 + (highPass ? c : -c)) * 0.5;
        b0 = first / a;
        b1 = (highPass ? -2.0 : 2.0) * first / a;
        b2 = first / a;
        a1 = (-2.0 * c) / a;
        a2 = (1.0 - alpha) / a;
    }

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

// Used by the desktop and browser EQ response displays. 0 dB excludes channel gain.
inline double responseDb (const ChannelSettings& settings, double sampleRate, double frequencyHz) noexcept
{
    double magnitude = 1.0;
    PeakBiquad stage;
    const auto processCutoff = [&] (const CutoffSettings& cfg, bool hp)
    {
        if (!cfg.enabled.load()) return;
        const double f = cfg.frequencyHz.load();
        const bool steep = cfg.slopeDbPerOct.load() == 24;
        const double q0 = steep ? 0.5411961001462 : 0.7071067811865;
        if (hp) stage.setHighPass(sampleRate, f, q0); else stage.setLowPass(sampleRate, f, q0);
        magnitude *= stage.magnitudeAt(sampleRate, frequencyHz);
        if (steep)
        {
            if (hp) stage.setHighPass(sampleRate, f, 1.3065629648764);
            else stage.setLowPass(sampleRate, f, 1.3065629648764);
            magnitude *= stage.magnitudeAt(sampleRate, frequencyHz);
        }
    };
    processCutoff(settings.highPass, true);
    for (const auto& eq : settings.eq)
    {
        stage.setPeak(sampleRate, eq.frequencyHz.load(), eq.gainDb.load(), eq.q.load());
        magnitude *= stage.magnitudeAt(sampleRate, frequencyHz);
    }
    processCutoff(settings.lowPass, false);
    return 20.0 * std::log10(std::max(1.0e-12, magnitude));
}

class MonoProcessor
{
public:
    void prepare (double newSampleRate, bool initiallyMuted = false)
    {
        sampleRate = std::max(8000.0, newSampleRate);
        delayBuffer.assign(static_cast<std::size_t>(std::ceil(sampleRate * kMaxDelayMs / 1000.0)) + 8, 0.0f);
        writeIndex = 0;
        for (auto& filter : filters) filter.reset();
        for (auto& filter : cutoffFilters) filter.reset();
        lastCutoff = { -1, -1, -1, -1 };
        for (auto& cached : oldValues) cached = { -1.0, -999.0, -1.0 };
        gainRamp.prepare(sampleRate, 0.015, 1.0);
        polarityRamp.prepare(sampleRate, 0.003, 1.0);
        muteRamp.prepare(sampleRate, 0.006, initiallyMuted ? 0.0 : 1.0); // Click-free 6 ms mute/unmute.
        delayRamp.prepare(sampleRate, 0.035, 0.0);
    }

    // Called once per audio buffer. No allocations in the processing callback.
    void beginBlock (const ChannelSettings& settings) noexcept
    {
        for (int i = 0; i < kBands; ++i)
        {
            const auto& band = settings.eq[static_cast<std::size_t>(i)];
            const double freq = band.frequencyHz.load(std::memory_order_relaxed);
            const double gain = band.gainDb.load(std::memory_order_relaxed);
            const double q = band.q.load(std::memory_order_relaxed);
            auto& previous = oldValues[static_cast<std::size_t>(i)];
            if (previous[0] != freq || previous[1] != gain || previous[2] != q)
            {
                filters[static_cast<std::size_t>(i)].setPeak(sampleRate, freq, gain, q);
                previous = { freq, gain, q };
            }
        }
        // A 4th-order Butterworth cascade needs Q=0.541196 and 1.306563.
        // For 12 dB/oct, use one Q=1/sqrt(2) stage instead.
        for (int mode = 0; mode < 2; ++mode)
        {
            const auto& filter = mode == 0 ? settings.highPass : settings.lowPass;
            const double hz = std::clamp(filter.frequencyHz.load(std::memory_order_relaxed), 20.0, 20000.0);
            const int slope = filter.slopeDbPerOct.load(std::memory_order_relaxed) == 24 ? 24 : 12;
            auto& state = lastCutoff[static_cast<std::size_t>(mode)];
            auto& stateSlope = lastCutoff[static_cast<std::size_t>(mode + 2)];
            if (state != hz || stateSlope != slope)
            {
                auto& f0 = cutoffFilters[static_cast<std::size_t>(mode * 2)];
                auto& f1 = cutoffFilters[static_cast<std::size_t>(mode * 2 + 1)];
                const double q0 = slope == 24 ? 0.5411961001462 : 0.7071067811865;
                const double q1 = 1.3065629648764;
                if (mode == 0) { f0.setHighPass(sampleRate, hz, q0); f1.setHighPass(sampleRate, hz, q1); }
                else { f0.setLowPass(sampleRate, hz, q0); f1.setLowPass(sampleRate, hz, q1); }
                state = hz;
                stateSlope = static_cast<double>(slope);
            }
            cutoffEnabled[static_cast<std::size_t>(mode)] = filter.enabled.load(std::memory_order_relaxed);
            cutoffStages[static_cast<std::size_t>(mode)] = slope == 24 ? 2 : 1;
        }
        gainRamp.setTarget(dbToGain(settings.levelDb.load(std::memory_order_relaxed)));
        polarityRamp.setTarget(settings.invertPolarity.load(std::memory_order_relaxed) ? -1.0 : 1.0);
        muteRamp.setTarget(settings.outputMuted.load(std::memory_order_relaxed) ? 0.0 : 1.0);
        delayRamp.setTarget(std::clamp(settings.delayMs.load(std::memory_order_relaxed), 0.0, kMaxDelayMs)
                            * sampleRate / 1000.0);
    }

    float process (float input) noexcept
    {
        float x = input;
        if (cutoffEnabled[0])
            for (int i = 0; i < cutoffStages[0]; ++i) x = cutoffFilters[static_cast<std::size_t>(i)].process(x);
        for (auto& filter : filters) x = filter.process(x);
        if (cutoffEnabled[1])
            for (int i = 0; i < cutoffStages[1]; ++i) x = cutoffFilters[static_cast<std::size_t>(i + 2)].process(x);

        delayBuffer[writeIndex] = x;
        const double lag = delayRamp.next();
        double readPosition = static_cast<double>(writeIndex) - lag;
        if (readPosition < 0.0) readPosition += static_cast<double>(delayBuffer.size());
        const auto earlier = static_cast<std::size_t>(readPosition);
        const auto later = (earlier + 1) % delayBuffer.size();
        const float fraction = static_cast<float>(readPosition - static_cast<double>(earlier));
        const float delayed = delayBuffer[earlier] * (1.0f - fraction) + delayBuffer[later] * fraction;
        writeIndex = (writeIndex + 1) % delayBuffer.size();
        return delayed * static_cast<float>(gainRamp.next() * polarityRamp.next() * muteRamp.next());
    }

private:
    double sampleRate = 48000.0;
    std::array<PeakBiquad, kBands> filters;
    std::array<PeakBiquad, 4> cutoffFilters;
    std::array<double, 4> lastCutoff { -1, -1, -1, -1 };
    std::array<bool, 2> cutoffEnabled { false, false };
    std::array<int, 2> cutoffStages { 1, 1 };
    std::array<std::array<double, 3>, kBands> oldValues {};
    std::vector<float> delayBuffer;
    std::size_t writeIndex = 0;
    ParameterRamp gainRamp, polarityRamp, delayRamp, muteRamp;
};
} // namespace dualdsp
