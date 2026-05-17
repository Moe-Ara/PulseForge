#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace AudioConfig {

inline constexpr uint32_t channelCount = 2;
inline constexpr uint32_t defaultSampleRate = 48000;
inline constexpr std::size_t ringBufferFrames = 48000;
inline constexpr std::size_t scratchFrames = 4096;

inline constexpr std::string_view pactlExecutable = "pactl";
inline constexpr std::string_view nullSinkModule = "module-null-sink";
inline constexpr std::string_view loopbackModule = "module-loopback";

inline constexpr std::string_view virtualSinkName = "pulseforge_enhanced";
inline constexpr std::string_view virtualSinkDisplayName =
    "PulseForge Enhanced";
inline constexpr std::string_view monitorSourceName =
    "pulseforge_enhanced.monitor";
inline constexpr int loopbackLatencyMs = 20;

inline constexpr std::string_view captureStreamName = "PulseForge Capture";
inline constexpr std::string_view playbackStreamName = "PulseForge Playback";
inline constexpr std::string_view processorThreadLoopName =
    "pulseforge-audio-processor";
inline constexpr std::string_view streamCaptureSinkProperty =
    "stream.capture.sink";
inline constexpr std::string_view nodeAutoconnectProperty = "node.autoconnect";
inline constexpr std::string_view nodeDontFallbackProperty =
    "node.dont-fallback";
inline constexpr std::string_view nodeDontMoveProperty = "node.dont-move";
inline constexpr std::string_view mediaTypeAudio = "Audio";
inline constexpr std::string_view captureCategory = "Capture";
inline constexpr std::string_view playbackCategory = "Playback";
inline constexpr std::string_view mediaRoleMusic = "Music";

} // namespace AudioConfig
