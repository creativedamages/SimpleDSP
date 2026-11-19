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
    const int version = static_cast<int>(preset.getProperty("version", 0));
    const bool current = version == presetVersion && entries->size() == channelCount
        && preset.getProperty("product", {}).toString() == presetProduct;
    const bool legacy = version == 1 && entries->size() == 16;
    if (!current && !legacy) return false;
    for (const auto& channel : *entries)
        if (!channel.isObject()) return false;
    return true;
}
}
