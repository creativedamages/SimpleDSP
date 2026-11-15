#include "../Source/Analysis.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    constexpr double rate = 48000.0;
    dualdsp::AudioTelemetry telemetry;
    telemetry.prepare(rate);
    dualdsp::SpectrumAnalyzer analyzer;
    std::vector<float> in(512), out(512);
    for (int block = 0; block < 30; ++block)
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
    for (int i = 0; i < 8; ++i) analyzer.update(telemetry, rate);
    const auto spectrum = analyzer.getBins();
    auto maxBin = 0;
    for (int i = 1; i < dualdsp::kRtaBins; ++i)
        if (spectrum[i] > spectrum[maxBin]) maxBin = i;
    const auto dominantHz = 20.0 * std::pow(std::min(20000.0, rate * 0.49) / 20.0,
                                           static_cast<double>(maxBin) / (dualdsp::kRtaBins-1));
    assert(dominantHz > 850 && dominantHz < 1200);
    assert(spectrum[maxBin] > -13.0f);
    assert(spectrum[0] < -45.0f);
    // No signal must produce floor values, not undefined FFT magnitudes.
    telemetry.recordBlock(nullptr, nullptr, 512);
    auto silent = telemetry.takeLevels();
    assert(silent.inputPeak <= -95.0f && silent.outputPeak <= -95.0f);
    std::cout << "Input/output peak/RMS and 1 kHz output RTA FFT tests passed.\n";
}
