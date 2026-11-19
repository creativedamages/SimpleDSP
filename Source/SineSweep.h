#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace htdsp
{
// The settling interval flushes the maximum 50 ms channel delay before
// opening the swept output, and retains its tail before restoring live audio.
class SineSweep
{
public:
    void start(double rate, double level)
    {
        sampleRate = std::max(8000.0, rate);
        position = 0;
        phase = 0;
        preRoll = static_cast<std::int64_t>(0.6 * sampleRate);
        toneLength = static_cast<std::int64_t>(10.0 * sampleRate);
        toneEnd = preRoll + toneLength;
        end = toneEnd + preRoll;
        stopping = false;
        fadeSamples = std::max<std::int64_t>(1, std::llround(sampleRate * 0.010));
        amplitude = targetAmplitude = std::pow(10.0, std::clamp(level, -90.0, 0.0) / 20.0);
        active = true;
    }
    void setLevel(double level) { targetAmplitude = std::pow(10.0, std::clamp(level, -90.0, 0.0) / 20.0); }
    void stop()
    {
        if (!active || stopping || position >= toneEnd) return;
        stopping = true;
        // Fade the remaining tone before letting the channel delay drain.
        toneEnd = std::min(toneEnd, position < preRoll ? position : position + fadeSamples);
        end = std::max(toneEnd, preRoll) + preRoll;
    }
    float liveInputGain() const
    {
        return static_cast<float>(std::clamp(1.0 - static_cast<double>(position) / fadeSamples, 0.0, 1.0));
    }

    float next()
    {
        if (!active) return 0.0f;
        amplitude += (targetAmplitude - amplitude) / (sampleRate * 0.01);
        const auto tonePosition = position - preRoll;
        double sample = 0.0;
        if (tonePosition >= 0 && position < toneEnd)
        {
            const double progress = static_cast<double>(tonePosition) / (toneLength - 1);
            const double frequency = 20.0 * std::pow(std::min(20000.0, sampleRate * 0.45) / 20.0, progress);
            const double fade = std::min({1.0, static_cast<double>(tonePosition) / fadeSamples,
                static_cast<double>(toneEnd - 1 - position) / fadeSamples});
            sample = std::sin(phase) * amplitude * fade;
            phase += 6.283185307179586 * frequency / sampleRate;
            if (phase >= 6.283185307179586) phase -= 6.283185307179586;
        }
        ++position;
        if (position >= end) active = false;
        return static_cast<float>(sample);
    }
    float outputGain(bool selected) const
    {
        if (!selected || position < fadeSamples) return liveInputGain();
        if (position < preRoll) return 0.0f;
        return static_cast<float>(std::clamp((end - position) / (sampleRate * 0.01), 0.0, 1.0));
    }
    bool allowsUnmute() const { return position >= fadeSamples; }
    bool running() const { return active; }
private:
    double sampleRate = 48000, phase = 0, amplitude = 0, targetAmplitude = 0;
    std::int64_t position = 0, preRoll = 0, toneLength = 0, toneEnd = 0, end = 0, fadeSamples = 1;
    bool active = false, stopping = false;
};
}
