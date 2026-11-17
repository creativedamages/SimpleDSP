#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace htdsp
{
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
        phase = gain = targetGain = 0.0;
        rampRemaining = 0;
        rows.fill(0.0);
        sum = 0.0;
        counter = 0;
        for (auto& row : rows) { row = white(); sum += row; }
    }

    void beginBlock(const TestSignalSettings& settings)
    {
        mode = std::clamp(settings.mode.load(), 0, 2);
        frequency = mode == 2 ? 1000.0 : std::clamp(settings.frequency.load(), 20.0, std::min(20000.0, sampleRate * 0.45));
        const double nextGain = settings.enabled.load() ? std::pow(10.0, std::clamp(settings.levelDb.load(), -90.0, 0.0) / 20.0) : 0.0;
        if (nextGain == targetGain) return;
        targetGain = nextGain;
        gainStep = (targetGain - gain) / std::max(1.0, sampleRate * 0.01);
        rampRemaining = static_cast<int>(std::max(1.0, sampleRate * 0.01));
    }

    float next()
    {
        if (rampRemaining > 0) { gain += gainStep; --rampRemaining; }
        else gain = targetGain;
        // Voss-McCartney pink noise: independently update octave-rate random
        // sources and add white noise to fill the highest octave.
        ++counter;
        unsigned int index = 0, bits = counter;
        while ((bits & 1u) == 0 && index < rows.size() - 1) { ++index; bits >>= 1u; }
        sum -= rows[index];
        rows[index] = white();
        sum += rows[index];
        const double pink = (sum + white()) / (rows.size() + 1.0);
        const double sine = std::sin(phase);
        phase += 6.283185307179586 * frequency / sampleRate;
        if (phase >= 6.283185307179586) phase -= 6.283185307179586;
        return static_cast<float>((mode == 0 ? pink : sine) * gain);
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
    double gain = 0, targetGain = 0, gainStep = 0;
    int mode = 0, rampRemaining = 0;
};
}
