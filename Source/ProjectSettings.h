#pragma once

#include <array>

namespace htdsp
{
inline constexpr int channelCount = 16;
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
bool hasValidChannels(const Value& channels)
{
    const auto* entries = channels.getArray();
    if (entries == nullptr || entries->size() != channelCount) return false;
    for (const auto& channel : *entries)
        if (!channel.isObject()) return false;
    return true;
}
}
