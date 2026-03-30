# VR Module - Stub placeholder
# This file will be replaced when we implement this module

add_library(RiftCore_VR SHARED
    src/placeholder.cpp
)

target_link_libraries(RiftCore_VR
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_VR
    PRIVATE VR_EXPORTS
)

target_include_directories(RiftCore_VR
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

message(STATUS "[Module] VR stub registered.")
