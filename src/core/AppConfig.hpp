#pragma once

#include <array>
#include <string_view>

namespace AppConfig {

inline constexpr std::string_view organizationName = "PulseForge";
inline constexpr std::string_view applicationName = "PulseForge";
inline constexpr std::string_view applicationVersion = "0.1.0-alpha";
inline constexpr std::string_view applicationId = "io.github.MoeAra.PulseForge";
inline constexpr std::string_view desktopFileName = "pulseforge";
inline constexpr std::string_view executableName = "pulseforge";
inline constexpr std::string_view systemdServiceName = "pulseforge.service";

inline constexpr std::string_view systemctlExecutable = "systemctl";

inline constexpr int processTimeoutMs = 3000;
inline constexpr int processKillWaitMs = 1000;
inline constexpr int processPollStepMs = 50;

inline constexpr std::array<std::string_view, 5> executableFallbackDirectories{
    "/usr/bin", "/bin", "/usr/local/bin", "/usr/sbin",
    "/run/current-system/sw/bin"};

inline constexpr std::string_view runtimeDirEnvironmentVariable =
    "XDG_RUNTIME_DIR";
inline constexpr std::string_view runtimeStateRelativePath =
    "pulseforge/runtime-state";
inline constexpr std::string_view runtimeStateFallbackPath =
    "/tmp/pulseforge-runtime-state";

inline constexpr std::string_view qssDevelopmentRelativePath =
    "/../src/gui/styleDark.qss";
inline constexpr std::string_view qssFileName = "styleDark.qss";

} // namespace AppConfig
