#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "DSP.h"

namespace htdsp
{
// Keep live outputs silent for the duration of the test and its delay tail.
class GeneratorIsolation
{
public:
    void prepare(double rate, bool initiallyEnabled = false) noexcept
    {
        liveFade.prepare(rate, 0.010, initiallyEnabled ? 0.0 : 1.0);
        drainSamples = static_cast<int>(std::ceil(rate * (0.010 + dualdsp::kMaxDelayMs / 1000.0)));
        enabled = initiallyEnabled;
        hadPlayback = false;
        remaining = 0;
    }

    void beginBlock(bool requested) noexcept
    {
        if (requested == enabled) return;
        enabled = requested;
        if (enabled)
        {
            liveFade.setTarget(0.0);
            remaining = 0;
        }
        else if (hadPlayback) remaining = drainSamples;
        else liveFade.setTarget(1.0);
    }

    float next() noexcept
    {
        if (remaining > 0 && --remaining == 0)
        {
            hadPlayback = false;
            liveFade.setTarget(1.0);
        }
        const double live = liveFade.next();
        if (enabled && live == 0.0) hadPlayback = true;
        return static_cast<float>(live);
    }

private:
    dualdsp::ParameterRamp liveFade;
    int drainSamples = 0, remaining = 0;
    bool enabled = false, hadPlayback = false;
};

struct TestSignalSettings
{
    std::atomic<bool> enabled { false };
    std::atomic<int> mode { 0 }; // Pink noise, sine oscillator, fixed 1 kHz.
    std::atomic<double> frequency { 1000.0 };
    std::atomic<double> levelDb { -40.0 };
};

class TestSignalGenerator
{
public:
    void prepare(double rate)
    {
        sampleRate = std::max(8000.0, rate);
        phase = requestedGain = 0.0;
        mode = requestedMode = 0;
        gain.prepare(sampleRate, 0.010, 0.0);
        rows.fill(0.0);
        sum = 0.0;
        counter = 0;
        for (auto& row : rows) { row = white(); sum += row; }
    }

    void beginBlock(const TestSignalSettings& settings)
    {
        requestedMode = std::clamp(settings.mode.load(), 0, 2);
        frequency = std::clamp(settings.frequency.load(), 20.0, std::min(20000.0, sampleRate * 0.45));
        requestedGain = settings.enabled.load() ? dualdsp::dbToGain(std::clamp(settings.levelDb.load(), -90.0, 0.0)) : 0.0;
    }

    float next(bool permitPlayback = true)
    {
        gain.setTarget(permitPlayback && mode == requestedMode ? requestedGain : 0.0);
        const double level = gain.next();
        if (level == 0.0)
        {
            mode = requestedMode;
            return 0.0f;
        }
        if (mode != 0)
        {
            const double sine = std::sin(phase);
            phase += 2.0 * dualdsp::kPi * (mode == 2 ? 1000.0 : frequency) / sampleRate;
            if (phase >= 2.0 * dualdsp::kPi) phase -= 2.0 * dualdsp::kPi;
            return static_cast<float>(sine * level);
        }
        // Voss-McCartney pink noise: independently update octave-rate random
        // sources and add white noise to fill the highest octave.
        ++counter;
        unsigned int index = 0, bits = counter;
        while ((bits & 1u) == 0 && index < rows.size() - 1) { ++index; bits >>= 1u; }
        sum -= rows[index];
        rows[index] = white();
        sum += rows[index];
        const double pink = (sum + white()) / (rows.size() + 1.0);
        return static_cast<float>(pink * level);
    }

private:
    double white()
    {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        return static_cast<double>(random) / 2147483648.0 - 1.0;
    }
    std::uint32_t random = 0x9e3779b9u, counter = 0;
    std::array<double, 16> rows {};
    double sampleRate = 48000, frequency = 1000, phase = 0, sum = 0;
    double requestedGain = 0.0;
    dualdsp::ParameterRamp gain;
    int mode = 0, requestedMode = 0;
};
}
