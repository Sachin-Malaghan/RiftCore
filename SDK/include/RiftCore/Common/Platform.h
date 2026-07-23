#pragma once

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define RIFTCORE_PLATFORM_WINDOWS 1
    #define RIFTCORE_PLATFORM_NAME "Windows"
#elif defined(__linux__)
    #define RIFTCORE_PLATFORM_LINUX 1
    #define RIFTCORE_PLATFORM_NAME "Linux"
#elif defined(__APPLE__)
    #define RIFTCORE_PLATFORM_MACOS 1
    #define RIFTCORE_PLATFORM_NAME "macOS"
#endif

// Architecture detection
#if defined(_M_X64) || defined(__x86_64__)
    #define RIFTCORE_ARCH_X64 1
#endif







// Compiler detection
#if defined(_MSC_VER)
    #define RIFTCORE_COMPILER_MSVC 1
#elif defined(__clang__)
    #define RIFTCORE_COMPILER_CLANG 1
#elif defined(__GNUC__)
    #define RIFTCORE_COMPILER_GCC 1
#endif

// DLL macros
#if defined(RIFTCORE_PLATFORM_WINDOWS)
    #define RIFTCORE_EXPORT  __declspec(dllexport)
    #define RIFTCORE_IMPORT  __declspec(dllimport)
#else
    #define RIFTCORE_EXPORT  __attribute__((visibility("default")))
    #define RIFTCORE_IMPORT  __attribute__((visibility("default")))
#endif

// Force inline
#if defined(_MSC_VER)
    #define RIFTCORE_FORCEINLINE __forceinline
#else
    #define RIFTCORE_FORCEINLINE __attribute__((always_inline)) inline
#endif

// Debug break
#if defined(_MSC_VER)
    #define RIFTCORE_DEBUGBREAK() __debugbreak()
#else
    #include <signal.h>
    #define RIFTCORE_DEBUGBREAK() raise(SIGTRAP)
#endif

// Assertions
#include <cassert>
#define RIFTCORE_ASSERT(expr)           assert(expr)
#define RIFTCORE_ASSERT_MSG(expr, msg)  assert((expr) && (msg))

// Unused variable
#define RIFTCORE_UNUSED(x) (void)(x)

// No copy / no move
#define RIFTCORE_NOCOPY(ClassName) \
    ClassName(const ClassName&)            = delete; \
    ClassName& operator=(const ClassName&) = delete

#define RIFTCORE_NOMOVE(ClassName) \
    ClassName(ClassName&&)            = delete; \
    ClassName& operator=(ClassName&&) = delete

#define RIFTCORE_NOCOPY_NOMOVE(ClassName) \
    RIFTCORE_NOCOPY(ClassName);           \
    RIFTCORE_NOMOVE(ClassName)

// Factory function helpers
#define RIFTCORE_FACTORY_FUNC(InterfaceType, FuncName) \
    extern "C" RIFTCORE_EXPORT InterfaceType* FuncName()

#define RIFTCORE_DESTROY_FUNC(InterfaceType, FuncName) \
    extern "C" RIFTCORE_EXPORT void FuncName(InterfaceType* ptr)
