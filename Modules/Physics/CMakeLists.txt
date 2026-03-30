# Physics Module - Stub placeholder
# This file will be replaced when we implement this module

add_library(RiftCore_Physics SHARED
    src/placeholder.cpp
)

target_link_libraries(RiftCore_Physics
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_Physics
    PRIVATE PHYSICS_EXPORTS
)

target_include_directories(RiftCore_Physics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

message(STATUS "[Module] Physics stub registered.")
