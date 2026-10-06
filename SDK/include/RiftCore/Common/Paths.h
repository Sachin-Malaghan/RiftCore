#pragma once
// Locates the project root (the folder that contains "Assets/") by walking
// up from the executable, so the apps run from any build folder or install.
#include <filesystem>
#include <string>
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

namespace RiftCore::Paths {

    inline std::filesystem::path ExecutableDir() {
#ifdef _WIN32
        char buf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        return std::filesystem::path(buf).parent_path();
#else
        std::error_code ec;
        auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
        return ec ? std::filesystem::current_path() : p.parent_path();
#endif
    }

    // Returns the first ancestor of the executable (or of the working
    // directory) that has an Assets folder; empty if none is found.
    inline std::filesystem::path FindProjectRoot() {
        std::error_code ec;
        std::filesystem::path starts[2] = {
            ExecutableDir(), std::filesystem::current_path(ec) };
        for (auto& start : starts) {
            for (auto p = start; !p.empty(); p = p.parent_path()) {
                if (std::filesystem::is_directory(p / "Assets", ec)) return p;
                if (p == p.parent_path()) break;
            }
        }
        return {};
    }

    // Makes "Assets/..." relative paths valid. Returns false if not found.
    inline bool EnterProjectRoot() {
        auto root = FindProjectRoot();
        if (root.empty()) return false;
        std::error_code ec;
        std::filesystem::current_path(root, ec);
        return !ec;
    }

} // namespace RiftCore::Paths
