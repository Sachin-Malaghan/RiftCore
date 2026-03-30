# Scene Module - Stub placeholder
# This file will be replaced when we implement this module

add_library(RiftCore_Scene SHARED
    src/placeholder.cpp
)

target_link_libraries(RiftCore_Scene
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_Scene
    PRIVATE SCENE_EXPORTS
)

target_include_directories(RiftCore_Scene
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

message(STATUS "[Module] Scene stub registered.")
