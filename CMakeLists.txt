cmake_minimum_required(VERSION 3.20)


# vcpkg toolchain - must be before project()
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
elseif(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE
        "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()


project(RiftCore
    VERSION 0.1.0
    DESCRIPTION "RiftCore Modular Game Engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib")

if(WIN32)
    message(STATUS "[RiftCore] Platform: Windows")
    add_compile_definitions(RIFTCORE_PLATFORM_WINDOWS)
elseif(UNIX AND NOT APPLE)
    message(STATUS "[RiftCore] Platform: Linux")
    add_compile_definitions(RIFTCORE_PLATFORM_LINUX)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
message(STATUS "[RiftCore] Build type: ${CMAKE_BUILD_TYPE}")

if(MSVC)
    add_compile_options(/W4 /MP /std:c++17)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -std=c++17)
endif()

include_directories("${CMAKE_SOURCE_DIR}/SDK/include")

add_subdirectory(ThirdParty)
add_subdirectory(SDK)
add_subdirectory(Modules/Core)
add_subdirectory(Modules/JobSystem)
add_subdirectory(Modules/ECS)
add_subdirectory(Modules/RHI)
add_subdirectory(Modules/OpenGLBackend)
add_subdirectory(Modules/Renderer)
add_subdirectory(Modules/Physics)
add_subdirectory(Modules/Input)
add_subdirectory(Modules/Audio)
add_subdirectory(Modules/Asset)
add_subdirectory(Modules/Scene)
add_subdirectory(Modules/Scripting)
add_subdirectory(Modules/VR)
add_subdirectory(Runtime)
add_subdirectory(Editor)
add_subdirectory(Tests)

message(STATUS "[RiftCore] Configuration complete.")

# Automatically set RiftCoreEditor as the highlighted Startup Project in Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RiftCoreEditor)

