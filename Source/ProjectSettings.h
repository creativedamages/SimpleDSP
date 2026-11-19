#pragma once

#include <array>

namespace htdsp
{
inline constexpr int channelCount = 2;
inline constexpr int presetVersion = 2;
inline constexpr auto presetProduct = "HTDSP";
inline constexpr int remotePort = 8766;
inline constexpr int maxPresetBytes = 256 * 1024;

inline bool outputIsAssigned(const std::array<int, channelCount>& routes, int channel, int output)
{
    if (output < 0) return false;
    for (int i = 0; i < channelCount; ++i)
        if (i != channel && routes[i] == output) return true;
    return false;
}

template <class Value>
bool hasValidPreset(const Value& preset)
{
    const auto channels = preset.getProperty("channels", {});
    const auto* entries = channels.getArray();
    if (entries == nullptr) return false;
    const auto versionField = preset.getProperty("version", {});
    if (!versionField.isInt() && !versionField.isInt64()) return false;
    const auto version = static_cast<long long>(versionField);
    if (preset.hasProperty("product"))
    {
        if (version != presetVersion || entries->size() != channelCount
            || preset.getProperty("product", {}).toString() != presetProduct)
            return false;
    }
    else
    {
        // The original SimpleDSP exported two channels as v2 without a product tag.
        // HT DSP exported 16 channels as v1, also without a tag.
        if (!((version == 2 && entries->size() == 2) || (version == 1 && entries->size() == 16)))
            return false;
    }
    for (const auto& channel : *entries)
        if (!channel.isObject()) return false;
    return true;
}
}
