#include "../Source/DSP.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    using namespace dualdsp;
    ChannelSettings settings;
    MonoProcessor processor;
    processor.prepare(48000.0);
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
    processor.prepare(48000.0);
    processor.beginBlock(settings);
    for (int n = 0; n < 2500; ++n) processor.process(0.0f); // settle delay ramp
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
    std::cout << "DSP passthrough, polarity, and 10ms delay tests passed.\n";
}
