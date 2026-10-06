cmake_minimum_required(VERSION 3.24)

# All third-party code is vendored under ThirdParty/ - no package manager needed.

project(RiftCore
    VERSION 0.2.0
    DESCRIPTION "RiftCore Modular Game Engine"
    LANGUAGES C CXX
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
    add_compile_options(/W4 /MP /std:c++17 /utf-8)
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
option(RIFTCORE_BUILD_EDITOR "Build the RiftCore Editor" ON)
if(RIFTCORE_BUILD_EDITOR)
    add_subdirectory(Editor)
endif()
enable_testing()
add_subdirectory(Tests)

message(STATUS "[RiftCore] Configuration complete.")

# RiftCoreEditor is the startup project in Visual Studio
if(RIFTCORE_BUILD_EDITOR)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RiftCoreEditor)
endif()

