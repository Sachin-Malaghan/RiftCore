// ============================================================
// Version.h — Engine version information
// ============================================================
#pragma once

#include "Types.h"

namespace RiftCore {

    struct EngineVersion {
        u32    major      = 0;
        u32    minor      = 1;
        u32    patch      = 0;
        String buildType  = "Debug";
        String commitHash = "unknown";

        String ToString() const {
            return std::to_string(major) + "." +
                   std::to_string(minor) + "." +
                   std::to_string(patch) + "-" +
                   buildType;
        }

        static EngineVersion Current() {
            return { 0, 1, 0, "Debug", "dev" };
        }
    };

    // Module API version — used for compatibility checks
    // When you load a DLL, you verify its API version matches
    constexpr u32 RIFTCORE_API_VERSION_MAJOR = 0;
    constexpr u32 RIFTCORE_API_VERSION_MINOR = 1;
    constexpr u32 RIFTCORE_API_VERSION =
        (RIFTCORE_API_VERSION_MAJOR << 16) |
        (RIFTCORE_API_VERSION_MINOR);

} // namespace RiftCore