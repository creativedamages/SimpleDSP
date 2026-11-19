#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <vector>

namespace dualdsp
{
constexpr int kBands = 6;
constexpr double kMaxDelayMs = 50.0;
constexpr double kDelayStepMs = 0.25;

inline double normalizedDelayMs(double ms) noexcept
{
    return std::isfinite(ms) ? std::round(std::clamp(ms, 0.0, kMaxDelayMs) / kDelayStepMs) * kDelayStepMs : 0.0;
}
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

    bool isSmoothing() const noexcept { return remaining > 0; }

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

// A filter bank owns both its coefficients and its sample history.
class FilterChain
{
public:
    bool update(const ChannelSettings& settings, double sampleRate) noexcept
    {
        bool changed = false;
        eqCount = 0;
        for (std::size_t band = 0; band < filters.size(); ++band)
        {
            const auto& eq = settings.eq[band];
            const std::array<double, 3> next { eq.frequencyHz.load(std::memory_order_relaxed),
                                              eq.gainDb.load(std::memory_order_relaxed),
                                              eq.q.load(std::memory_order_relaxed) };
            if (next[1] != 0.0) activeEq[eqCount++] = band;
            if (next == eqValues[band]) continue;
            filters[band].setPeak(sampleRate, next[0], next[1], next[2]);
            filters[band].reset();
            eqValues[band] = next;
            changed = true;
        }
        changed |= updateCutoff(settings.highPass, highPass, sampleRate, true);
        changed |= updateCutoff(settings.lowPass, lowPass, sampleRate, false);
        return changed;
    }

    float process(float x) noexcept
    {
        for (int i = 0; i < highPass.count; ++i) x = highPass.stages[i].process(x);
        for (std::size_t i = 0; i < eqCount; ++i) x = filters[activeEq[i]].process(x);
        for (int i = 0; i < lowPass.count; ++i) x = lowPass.stages[i].process(x);
        return x;
    }

    // The EQ graph represents filter gain only.
    double responseDb(double sampleRate, double frequencyHz) const noexcept
    {
        double magnitude = 1.0;
        for (int i = 0; i < highPass.count; ++i) magnitude *= highPass.stages[i].magnitudeAt(sampleRate, frequencyHz);
        for (std::size_t i = 0; i < eqCount; ++i) magnitude *= filters[activeEq[i]].magnitudeAt(sampleRate, frequencyHz);
        for (int i = 0; i < lowPass.count; ++i) magnitude *= lowPass.stages[i].magnitudeAt(sampleRate, frequencyHz);
        return 20.0 * std::log10(std::max(1.0e-12, magnitude));
    }

private:
    struct Cutoff
    {
        std::array<PeakBiquad, 2> stages;
        double hz = 0.0;
        int count = 0;
    };

    static bool updateCutoff(const CutoffSettings& settings, Cutoff& cutoff, double rate, bool high) noexcept
    {
        const int count = !settings.enabled.load(std::memory_order_relaxed) ? 0
            : (settings.slopeDbPerOct.load(std::memory_order_relaxed) == 24 ? 2 : 1);
        const double hz = settings.frequencyHz.load(std::memory_order_relaxed);
        if (cutoff.count == count && (count == 0 || cutoff.hz == hz)) return false;
        cutoff.hz = hz;
        cutoff.count = count;
        for (int i = 0; i < count; ++i)
        {
            auto& stage = cutoff.stages[i];
            const double q = count == 1 ? 0.7071067811865 : (i == 0 ? 0.5411961001462 : 1.3065629648764);
            if (high) stage.setHighPass(rate, hz, q);
            else stage.setLowPass(rate, hz, q);
            stage.reset();
        }
        return true;
    }

    std::array<PeakBiquad, kBands> filters;
    std::array<std::array<double, 3>, kBands> eqValues {};
    std::array<std::size_t, kBands> activeEq {};
    std::size_t eqCount = 0;
    Cutoff highPass, lowPass;
};

inline double responseDb(const ChannelSettings& settings, double sampleRate, double frequencyHz) noexcept
{
    FilterChain filters;
    filters.update(settings, sampleRate);
    return filters.responseDb(sampleRate, frequencyHz);
}

class MonoProcessor
{
public:
    void prepare(double newSampleRate, const ChannelSettings& settings)
    {
        sampleRate = std::max(8000.0, newSampleRate);
        delayBuffer.assign(static_cast<std::size_t>(std::ceil(sampleRate * kMaxDelayMs / 1000.0)) + 1, 0.0f);
        writeIndex = 0;
        activeDelay = nextDelay = delaySamples(settings);
        activeFilters = 0;
        filters = {};
        filters[activeFilters].update(settings, sampleRate);
        gainRamp.prepare(sampleRate, 0.015, dbToGain(settings.levelDb.load()));
        polarityRamp.prepare(sampleRate, 0.003, settings.invertPolarity.load() ? -1.0 : 1.0);
        muteRamp.prepare(sampleRate, 0.006, settings.outputMuted.load() ? 0.0 : 1.0);
        filterFade.prepare(sampleRate, 0.030, 1.0);
        delayFade.prepare(sampleRate, 0.035, 1.0);
    }

    // Finish the current transition before applying the latest request. This keeps
    // rapid slider updates from interrupting a fade or resetting an audible filter.
    void beginBlock(const ChannelSettings& settings, bool forceUnmute = false) noexcept
    {
        if (!filterFade.isSmoothing())
        {
            auto& next = filters[1 - activeFilters];
            next = filters[activeFilters];
            if (next.update(settings, sampleRate))
            {
                filterFade.prepare(sampleRate, 0.030, 0.0);
                filterFade.setTarget(1.0);
            }
        }
        if (!delayFade.isSmoothing())
        {
            nextDelay = delaySamples(settings);
            if (nextDelay != activeDelay)
            {
                delayFade.prepare(sampleRate, 0.035, 0.0);
                delayFade.setTarget(1.0);
            }
        }
        gainRamp.setTarget(dbToGain(settings.levelDb.load(std::memory_order_relaxed)));
        polarityRamp.setTarget(settings.invertPolarity.load(std::memory_order_relaxed) ? -1.0 : 1.0);
        muteRamp.setTarget(!forceUnmute && settings.outputMuted.load(std::memory_order_relaxed) ? 0.0 : 1.0);
    }

    float process(float input) noexcept
    {
        float x = filters[activeFilters].process(input);
        if (filterFade.isSmoothing())
        {
            const float next = filters[1 - activeFilters].process(input);
            x = blend(x, next, filterFade.next());
            if (!filterFade.isSmoothing()) activeFilters = 1 - activeFilters;
        }

        delayBuffer[writeIndex] = x;
        float delayed = readDelay(activeDelay);
        if (delayFade.isSmoothing())
        {
            delayed = blend(delayed, readDelay(nextDelay), delayFade.next());
            if (!delayFade.isSmoothing()) activeDelay = nextDelay;
        }
        if (++writeIndex == delayBuffer.size()) writeIndex = 0;
        return delayed * static_cast<float>(gainRamp.next() * polarityRamp.next() * muteRamp.next());
    }

private:
    static float blend(float a, float b, double amount) noexcept
    {
        return static_cast<float>((1.0 - amount) * a + amount * b);
    }

    std::size_t delaySamples(const ChannelSettings& settings) const noexcept
    {
        return static_cast<std::size_t>(std::round(normalizedDelayMs(settings.delayMs.load(std::memory_order_relaxed))
                                                   * sampleRate / 1000.0));
    }

    float readDelay(std::size_t samples) const noexcept
    {
        const auto index = writeIndex >= samples ? writeIndex - samples : delayBuffer.size() - (samples - writeIndex);
        return delayBuffer[index];
    }

    double sampleRate = 48000.0;
    std::array<FilterChain, 2> filters;
    std::size_t activeFilters = 0;
    std::vector<float> delayBuffer;
    std::size_t writeIndex = 0, activeDelay = 0, nextDelay = 0;
    ParameterRamp gainRamp, polarityRamp, muteRamp, filterFade, delayFade;
};
} // namespace dualdsp
