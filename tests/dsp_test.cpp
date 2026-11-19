#include "../Source/DSP.h"
#include "../Source/TestSignal.h"
#include "../Source/SineSweep.h"
#include "../Source/ProjectSettings.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    static_assert(htdsp::channelCount == 2);
    std::array<int, htdsp::channelCount> routes;
    routes.fill(-1);
    routes[0] = 31;
    routes[1] = 62;
    assert(htdsp::outputIsAssigned(routes, 0, 62));
    assert(htdsp::outputIsAssigned(routes, 1, 31));
    assert(!htdsp::outputIsAssigned(routes, 1, 62));
    assert(!htdsp::outputIsAssigned(routes, 1, -1));


    htdsp::TestSignalSettings testSettings;
    htdsp::TestSignalGenerator generator;
    generator.prepare(48000);
    generator.beginBlock(testSettings);
    for (int i = 0; i < 1000; ++i) assert(generator.next() == 0.0f);
    testSettings.enabled.store(true);
    testSettings.mode.store(2);
    testSettings.frequency.store(123);
    testSettings.levelDb.store(-20);
    generator.beginBlock(testSettings);
    for (int i = 0; i < 4800; ++i) generator.next();
    double tonePower = 0;
    int crossings = 0;
    float previous = generator.next();
    for (int i = 0; i < 48000; ++i)
    {
        const float value = generator.next();
        assert(std::abs(value) <= 0.10001f);
        tonePower += value * value;
        if (previous <= 0 && value > 0) ++crossings;
        previous = value;
    }
    assert(crossings >= 999 && crossings <= 1001);
    assert(std::abs(tonePower / 48000 - 0.005) < 0.00001);
    testSettings.enabled.store(false);
    generator.beginBlock(testSettings);
    for (int i = 0; i < 1000; ++i) generator.next();
    assert(generator.next() == 0.0f);
    testSettings.enabled.store(true);
    testSettings.mode.store(0);
    generator.beginBlock(testSettings);
    double noisePower = 0;
    for (int i = 0; i < 48000; ++i)
    {
        const float value = generator.next();
        assert(std::isfinite(value) && std::abs(value) <= 0.10001f);
        noisePower += value * value;
    }
    assert(noisePower > 0.01);

    htdsp::SineSweep sweep;
    sweep.start(48000, -20);
    int sweepSamples = 0, earlyCrossings = 0, lateCrossings = 0;
    float priorSweep = 0;
    while (sweep.running())
    {
        const float value = sweep.next();
        assert(std::isfinite(value) && std::abs(value) <= 0.10001f);
        if (value != 0) assert(sweep.outputGain(false) == 0);
        if (sweepSamples < 28800) assert(value == 0);
        if (priorSweep <= 0 && value > 0)
        {
            if (sweepSamples >= 28800 && sweepSamples < 76800) ++earlyCrossings;
            if (sweepSamples >= 460800 && sweepSamples < 508800) ++lateCrossings;
        }
        priorSweep = value;
        ++sweepSamples;
    }
    assert(sweepSamples == 537600);
    assert(earlyCrossings > 20 && earlyCrossings < 40);
    assert(lateCrossings > 10000);
    assert(sweep.outputGain(true) == 0 && sweep.outputGain(false) == 0);
    sweep.start(48000, -40);
    for (int n = 0; n < 100000; ++n) sweep.next();
    sweep.stop();
    for (int n = 0; n < 480; ++n)
    {
        assert(std::abs(sweep.next()) <= 0.01001f);
        assert(sweep.outputGain(false) == 0);
    }
    int tailSamples = 0;
    while (sweep.running())
    {
        assert(sweep.next() == 0);
        assert(sweep.outputGain(false) == 0);
        ++tailSamples;
    }
    assert(tailSamples == 28800);
    using namespace dualdsp;
    ChannelSettings settings;
    MonoProcessor processor;
    settings.outputMuted.store(true);
    processor.prepare(48000.0, settings);
    processor.beginBlock(settings);
    assert(processor.process(1.0f) == 0.0f);
    settings.outputMuted.store(false);
    processor.prepare(48000.0, settings);
    processor.beginBlock(settings);
    // A unity channel with flat PEQs and zero delay must pass samples unchanged.
    for (int n = 0; n < 1024; ++n)
    {
        const float input = static_cast<float>(std::sin(2.0 * kPi * n * 440.0 / 48000.0));
        assert(std::abs(processor.process(input) - input) < 1e-5f);
    }
    settings.invertPolarity.store(true);
    processor.beginBlock(settings);
    for (int n = 0; n < 200; ++n) processor.process(1.0f);
    assert(std::abs(processor.process(0.5f) + 0.5f) < 1e-4f);

    settings.invertPolarity.store(false);
    settings.delayMs.store(10.0);
    processor.prepare(48000.0, settings);
    processor.beginBlock(settings);
    float at479 = 0, at480 = 0;
    for (int n = 0; n < 1000; ++n)
    {
        const float output = processor.process(n == 0 ? 1.0f : 0.0f);
        if (n == 479) at479 = output;
        if (n == 480) at480 = output;
    }
    assert(std::abs(at479) < 1e-5f);
    assert(std::abs(at480 - 1.0f) < 1e-4f);
    // Delay requests beyond the supported range must be clamped by DSP.
    static_assert(kMaxDelayMs == 50.0);
    settings.delayMs.store(9999.0);
    processor.prepare(48000.0, settings);
    processor.beginBlock(settings);
    for (int n = 0; n < 4800; ++n)
        assert(processor.process(n == 0 ? 1.0f : 0.0f) == (n == 2400 ? 1.0f : 0.0f));
    assert(normalizedDelayMs(10.01) == 10.0 && normalizedDelayMs(10.14) == 10.25);
    settings.delayMs.store(10.25);
    constexpr double rate = 44100;
    processor.prepare(rate, settings);
    for (int n = 0; n < 1000; ++n)
        assert(processor.process(n == 0 ? 1.0f : 0.0f) == (n == 452 ? 1.0f : 0.0f));
    // The graph calculation must match the high/low-pass coefficients in the DSP.
    settings.delayMs.store(0.0);
    settings.invertPolarity.store(false);
    settings.highPass.enabled.store(true);
    settings.highPass.frequencyHz.store(1000.0);
    settings.highPass.slopeDbPerOct.store(12);
    const auto hp12 = responseDb(settings, 48000.0, 100.0);
    settings.highPass.slopeDbPerOct.store(24);
    const auto hp24 = responseDb(settings, 48000.0, 100.0);
    assert(hp12 < -35.0 && hp24 < hp12 - 30.0);
    settings.highPass.enabled.store(false);
    settings.lowPass.enabled.store(true);
    settings.lowPass.frequencyHz.store(1000.0);
    settings.lowPass.slopeDbPerOct.store(12);
    const auto lp12 = responseDb(settings, 48000.0, 10000.0);
    settings.lowPass.slopeDbPerOct.store(24);
    const auto lp24 = responseDb(settings, 48000.0, 10000.0);
    assert(lp12 < -35.0 && lp24 < lp12 - 30.0);
    settings.lowPass.enabled.store(false);
    settings.eq[0].frequencyHz.store(1000.0);
    settings.eq[0].gainDb.store(6.0);
    assert(std::abs(responseDb(settings, 48000.0, 1000.0) - 6.0) < 0.01);
    // Verify real-time output actually attenuates low frequencies with HPF enabled.
    settings.eq[0].gainDb.store(0.0);
    settings.highPass.enabled.store(true);
    settings.highPass.frequencyHz.store(1000.0);
    settings.highPass.slopeDbPerOct.store(12);
    MonoProcessor highpass;
    highpass.prepare(48000.0, settings);
    highpass.beginBlock(settings);
    double energy = 0.0;
    for (int n = 0; n < 48000; ++n)
    {
        const float x = static_cast<float>(std::sin(2.0 * kPi * 100.0 * n / 48000.0));
        const float y = highpass.process(x);
        if (n > 24000) energy += static_cast<double>(y) * y;
    }
    assert(std::sqrt(energy / 23999.0) < 0.01); // far below normal 0.707 sine RMS
    settings.highPass.enabled.store(false);
    // Muting must be independent of the level setting, and must fade instead
    // of switching abruptly (to prevent clicks when used from a phone).
    ChannelSettings mutedSettings;
    MonoProcessor muteProcessor;
    muteProcessor.prepare(48000.0, mutedSettings);
    muteProcessor.beginBlock(mutedSettings);
    for (int n = 0; n < 400; ++n)
        assert(std::abs(muteProcessor.process(0.5f) - 0.5f) < 1e-4f);
    mutedSettings.outputMuted.store(true);
    muteProcessor.beginBlock(mutedSettings);
    const float firstMuted = muteProcessor.process(0.5f);
    assert(firstMuted > 0.0f && firstMuted < 0.5f);
    for (int n = 0; n < 400; ++n) muteProcessor.process(0.5f);
    assert(std::abs(muteProcessor.process(0.5f)) < 1e-5f);
    mutedSettings.outputMuted.store(false);
    muteProcessor.beginBlock(mutedSettings);
    for (int n = 0; n < 400; ++n) muteProcessor.process(0.5f);
    assert(std::abs(muteProcessor.process(0.5f) - 0.5f) < 1e-4f);
    // A large live retune previously reused the old filter state and exceeded full scale.
    ChannelSettings retuned;
    retuned.lowPass.enabled = true;
    retuned.lowPass.frequencyHz = 1000.0;
    MonoProcessor lowpass;
    lowpass.prepare(48000.0, retuned);
    float peak = 0.0f;
    double tailPower = 0.0;
    for (int n = 0; n < 96000; ++n)
    {
        if (n == 48000) { retuned.lowPass.frequencyHz = 20.0; lowpass.beginBlock(retuned); }
        const auto input = static_cast<float>(0.1 * std::sin(2 * kPi * 100.0 * n / 48000.0 + 0.6));
        const auto output = lowpass.process(input);
        peak = std::max(peak, std::abs(output));
        if (n >= 72000) tailPower += output * output;
    }
    assert(peak < 0.12f);
    assert(std::abs(std::sqrt(tailPower / 24000) - 0.002826) < 0.00005);

    // Test excitation must wait for the live outputs to fade, then leave time
    // for the longest delay to drain before those outputs return.
    htdsp::GeneratorIsolation isolation;
    isolation.prepare(48000);
    assert(isolation.next() == 1.0f);
    isolation.beginBlock(true);
    for (int n = 0; n < 479; ++n)
    {
        const auto gate = isolation.next();
        assert(gate > 0.0f && gate < 1.0f);
    }
    assert(isolation.next() == 0.0f);
    isolation.beginBlock(false);
    for (int n = 0; n < 2879; ++n) assert(isolation.next() == 0.0f);
    for (int n = 0; n < 479; ++n)
    {
        const auto gate = isolation.next();
        assert(gate > 0.0f && gate < 1.0f);
    }
    assert(isolation.next() == 1.0f);
    std::cout << "DSP routing, filters, delay, mute, generator isolation and sweep passed.\n";
}
