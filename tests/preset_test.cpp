#include <juce_core/juce_core.h>
#include "../Source/ProjectSettings.h"
#include <cassert>

namespace
{
juce::var parse(const juce::String& text)
{
    juce::var preset;
    const auto status = juce::JSON::parse(text, preset);
    assert(status.wasOk());
    return preset;
}
}

int main()
{
    using htdsp::hasValidPreset;
    // Older SimpleDSP exports have the same DSP fields but omit the product tag.
    const auto original = parse(R"({
        "version": 2, "device": "ASIO :: USB Audio",
        "channels": [
            {
                "name": "Left", "inputRoute": 0, "outputRoute": 0,
                "levelDb": -3.0, "delayMs": 2.75, "invertPolarity": false,
                "outputMuted": true,
                "eq": [{"frequencyHz": 80, "gainDb": -4.0, "q": 1.2}],
                "highPass": {"enabled": true, "frequency": 40, "slope": 24},
                "lowPass": {"enabled": false, "frequency": 16000, "slope": 12}
            },
            {"name": "Right", "inputRoute": 1, "outputRoute": 1}
        ]
    })");
    assert(hasValidPreset(original));
    assert(hasValidPreset(parse(R"({"version":2,"product":"HTDSP","channels":[{},{}]})")));

    juce::StringArray legacyChannels;
    for (int i = 0; i < 16; ++i) legacyChannels.add("{}");
    auto legacy = parse("{\"version\":1,\"channels\":[" + legacyChannels.joinIntoString(",") + "]}");
    assert(hasValidPreset(legacy));
    legacy.getDynamicObject()->setProperty("version", 2);
    assert(!hasValidPreset(legacy));

    assert(!hasValidPreset(parse(R"({"version":2,"product":"Unknown","channels":[{},{}]})")));
    assert(!hasValidPreset(parse(R"({"version":2,"product":null,"channels":[{},{}]})")));
    assert(!hasValidPreset(parse(R"({"version":2,"channels":[{},null]})")));
    assert(!hasValidPreset(parse(R"({"version":1,"channels":[{},{}]})")));
    assert(!hasValidPreset(parse(R"({"version":3,"channels":[{},{}]})")));
    assert(!hasValidPreset(parse(R"({"version":4294967298,"channels":[{},{}]})")));
    assert(!hasValidPreset(parse(R"({"channels":[{},{}]})")));
    assert(!hasValidPreset(parse(R"({"version":2,"channels":[{}]})")));
}
