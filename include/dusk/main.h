#ifndef DUSK_MAIN_H
#define DUSK_MAIN_H

#include <filesystem>

#if (defined(_WIN32) && !defined(_UWP)) ||                                                          \
    (defined(__APPLE__) && !TARGET_OS_IOS && !TARGET_OS_TV && !TARGET_OS_MACCATALYST) ||           \
    (defined(__linux__) && !defined(__ANDROID__))
#define DUSK_CAN_OPEN_DATA_FOLDER 1
#else
#define DUSK_CAN_OPEN_DATA_FOLDER 0
#endif

namespace dusk {

extern bool IsRunning;
extern bool IsShuttingDown;
extern bool IsGameLaunched;
extern bool RestartRequested;
extern std::filesystem::path ConfigPath;
extern std::filesystem::path CachePath;

#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS) ||                           \
    (defined(TARGET_OS_TV) && TARGET_OS_TV) || defined(_UWP)
inline constexpr bool SupportsProcessRestart = false;
#else
inline constexpr bool SupportsProcessRestart = true;
#endif

void RequestRestart() noexcept;

}  // namespace dusk

#endif  // DUSK_MAIN_H
