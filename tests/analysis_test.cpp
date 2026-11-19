#include "../Source/Analysis.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    constexpr double rate = 48000.0;
    static dualdsp::AudioTelemetry telemetry;
    telemetry.prepare(rate);
    static dualdsp::SpectrumAnalyzer analyzer;
    static dualdsp::SpectrumAnalyzer inputAnalyzer;
    std::vector<float> in(512), out(512);
    for (int block = 0; block < (dualdsp::kRtaFftSize + 511) / 512; ++block)
    {
        for (int n = 0; n < 512; ++n)
        {
            in[n] = static_cast<float>(0.7 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * (n + 512 * block) / rate));
            out[n] = in[n] * 0.5f;
        }
        telemetry.recordBlock(in.data(), out.data(), 512);
    }
    auto levels = telemetry.takeLevels();
    assert(levels.inputPeak > -3.3f && levels.inputPeak < -2.7f);
    assert(levels.outputPeak > -9.4f && levels.outputPeak < -8.7f);
    assert(levels.outputRms < levels.inputRms - 5.8f);
    for (int i = 0; i < 8; ++i)
    {
        analyzer.update(telemetry, rate);
        inputAnalyzer.update(telemetry, rate, true);
    }
    const auto spectrum = analyzer.getBins();
    auto maxBin = 0;
    for (int i = 1; i < dualdsp::kRtaBins; ++i)
        if (spectrum[i] > spectrum[maxBin]) maxBin = i;
    const auto dominantHz = 20.0 * std::pow(std::min(20000.0, rate * 0.49) / 20.0,
                                           static_cast<double>(maxBin) / (dualdsp::kRtaBins-1));
    assert(dominantHz > 850 && dominantHz < 1200);
    assert(spectrum[maxBin] > -13.0f);
    const auto inputSpectrum = inputAnalyzer.getBins();
    // The pre-DSP sine is twice the post-DSP sine (approximately +6.02 dB).
    assert(inputSpectrum[maxBin] > spectrum[maxBin] + 5.0f);
    assert(inputSpectrum[maxBin] < spectrum[maxBin] + 7.0f);
    assert(inputSpectrum[0] < -45.0f);
    assert(spectrum[0] < -45.0f);
    // No signal must produce floor values, not undefined FFT magnitudes.
    telemetry.recordBlock(nullptr, nullptr, 512);
    auto silent = telemetry.takeLevels();
    assert(silent.inputPeak <= -95.0f && silent.outputPeak <= -95.0f);
    // Clip lights latch until explicitly cleared. The pre-DSP input
    // and post-DSP output must latch independently.
    assert(!telemetry.takeLevels().inputClip && !telemetry.takeLevels().outputClip);
    float clippedInput[4] { 0.0f, -1.03f, 0.4f, 0.0f };
    float quietOutput[4] { 0.0f, 0.2f, 0.1f, 0.0f };
    telemetry.recordBlock(clippedInput, quietOutput, 4);
    assert(telemetry.takeLevels().inputClip);
    assert(!telemetry.takeLevels().outputClip);
    telemetry.recordBlock(nullptr, nullptr, 512);
    assert(telemetry.takeLevels().inputClip); // latched beyond peak-window refresh
    telemetry.clearClip(true);
    assert(!telemetry.takeLevels().inputClip);
    float clippedOutput[4] { 0.0f, 1.01f, 0.0f, 0.0f };
    telemetry.recordBlock(quietOutput, clippedOutput, 4);
    assert(telemetry.takeLevels().outputClip);
    assert(!telemetry.takeLevels().inputClip);
    telemetry.clearClip(false);
    assert(!telemetry.takeLevels().outputClip);

    // A quarter-rate sinusoid with samples at ±0.8 reaches ±1.131 between samples.
    static dualdsp::AudioTelemetry truePeak;
    truePeak.prepare(rate);
    for (int n = 0; n < 512; ++n) in[n] = n % 4 < 2 ? 0.8f : -0.8f;
    for (int block = 0; block < 4; ++block)
        truePeak.recordBlock(in.data(), in.data(), 512);
    const auto interSample = truePeak.takeLevels();
    assert(interSample.inputPeak < -1.9f && !interSample.inputClip);
    assert(interSample.outputPeak > 0.9f && interSample.outputPeak < 1.5f && interSample.outputClip);
    assert(dualdsp::rtaMaxHz(48000) == 20000 && dualdsp::rtaMaxHz(32000) == 15680);

    // With a 16k FFT at 48kHz, a 32Hz tone has ~3Hz native resolution
    // rather than the old ~12Hz, providing better bass trace detail.
    static_assert(dualdsp::kRtaFftSize >= 16384, "FFT must resolve low bass");
    static dualdsp::AudioTelemetry lowBass;
    lowBass.prepare(rate);
    static dualdsp::SpectrumAnalyzer bassAnalyzer;
    std::vector<float> bass(512);
    for (int block = 0; block < 48; ++block)
    {
        for (int n = 0; n < 512; ++n)
            bass[n] = static_cast<float>(0.5 * std::sin(
                2.0 * 3.14159265358979323846 * 32.0 * (n + 512 * block) / rate));
        lowBass.recordBlock(bass.data(), bass.data(), 512);
    }
    for (int n = 0; n < 8; ++n) bassAnalyzer.update(lowBass, rate);
    const auto bassBins = bassAnalyzer.getBins();
    int bassMax = 0;
    for (int n = 1; n < dualdsp::kRtaBins; ++n)
        if (bassBins[n] > bassBins[bassMax]) bassMax = n;
    const double bassPeak = 20.0 * std::pow(1000.0,
        static_cast<double>(bassMax) / (dualdsp::kRtaBins-1));
    assert(bassPeak > 27.0 && bassPeak < 40.0);
    std::cout << "Meters, true peak, independent clip latches and 32Hz/1kHz RTAs passed.\n";

}
