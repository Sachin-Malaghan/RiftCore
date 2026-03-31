add_library(RiftCore_Audio SHARED
    src/AudioSystem.cpp
)

target_compile_features(RiftCore_Audio
    PUBLIC cxx_std_17
)

target_include_directories(RiftCore_Audio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/ThirdParty/miniaudio
)

target_link_libraries(RiftCore_Audio
    PUBLIC  RiftCore_SDK
    PRIVATE miniaudio
)

target_compile_definitions(RiftCore_Audio
    PRIVATE AUDIO_EXPORTS
)

# Suppress warnings from miniaudio includes
if(MSVC)
    target_compile_options(RiftCore_Audio
        PRIVATE /W3)
    target_compile_definitions(RiftCore_Audio
        PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(RiftCore_Audio PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[Module] Audio registered.")
