# Audio Module - Stub placeholder
# This file will be replaced when we implement this module

add_library(RiftCore_Audio SHARED
    src/placeholder.cpp
)

target_link_libraries(RiftCore_Audio
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_Audio
    PRIVATE AUDIO_EXPORTS
)

target_include_directories(RiftCore_Audio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

message(STATUS "[Module] Audio stub registered.")
