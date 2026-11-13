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
constexpr double kMaxDelayMs = 2000.0;
constexpr double kPi = 3.1415926535897932384626433832795;

struct EqSettings
{
    std::atomic<double> frequencyHz { 1000.0 };
    std::atomic<double> gainDb { 0.0 };
    std::atomic<double> q { 1.0 };
};

struct ChannelSettings
{
    std::atomic<int> inputRoute { -1 };   // Physical, zero-based input port; -1 = unassigned.
    std::atomic<int> outputRoute { -1 };  // Physical, zero-based output port; -1 = unassigned.
    std::atomic<double> levelDb { 0.0 };
    std::atomic<double> delayMs { 0.0 };
    std::atomic<bool> invertPolarity { false };
    std::array<EqSettings, kBands> eq;

    ChannelSettings()
    {
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
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

class MonoProcessor
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = std::max(8000.0, newSampleRate);
        delayBuffer.assign(static_cast<std::size_t>(std::ceil(sampleRate * kMaxDelayMs / 1000.0)) + 8, 0.0f);
        writeIndex = 0;
        for (auto& filter : filters) filter.reset();
        for (auto& cached : oldValues) cached = { -1.0, -999.0, -1.0 };
        gainRamp.prepare(sampleRate, 0.015, 1.0);
        polarityRamp.prepare(sampleRate, 0.003, 1.0);
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
        gainRamp.setTarget(dbToGain(settings.levelDb.load(std::memory_order_relaxed)));
        polarityRamp.setTarget(settings.invertPolarity.load(std::memory_order_relaxed) ? -1.0 : 1.0);
        delayRamp.setTarget(std::clamp(settings.delayMs.load(std::memory_order_relaxed), 0.0, kMaxDelayMs)
                            * sampleRate / 1000.0);
    }

    float process (float input) noexcept
    {
        float x = input;
        for (auto& filter : filters) x = filter.process(x);

        delayBuffer[writeIndex] = x;
        const double lag = delayRamp.next();
        double readPosition = static_cast<double>(writeIndex) - lag;
        if (readPosition < 0.0) readPosition += static_cast<double>(delayBuffer.size());
        const auto earlier = static_cast<std::size_t>(readPosition);
        const auto later = (earlier + 1) % delayBuffer.size();
        const float fraction = static_cast<float>(readPosition - static_cast<double>(earlier));
        const float delayed = delayBuffer[earlier] * (1.0f - fraction) + delayBuffer[later] * fraction;
        writeIndex = (writeIndex + 1) % delayBuffer.size();
        return delayed * static_cast<float>(gainRamp.next() * polarityRamp.next());
    }

private:
    double sampleRate = 48000.0;
    std::array<PeakBiquad, kBands> filters;
    std::array<std::array<double, 3>, kBands> oldValues {};
    std::vector<float> delayBuffer;
    std::size_t writeIndex = 0;
    ParameterRamp gainRamp, polarityRamp, delayRamp;
};
} // namespace dualdsp
