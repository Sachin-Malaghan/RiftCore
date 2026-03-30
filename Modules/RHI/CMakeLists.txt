# RHI Module - Stub placeholder
# This file will be replaced when we implement this module

add_library(RiftCore_RHI SHARED
    src/placeholder.cpp
)

target_link_libraries(RiftCore_RHI
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_RHI
    PRIVATE RHI_EXPORTS
)

target_include_directories(RiftCore_RHI
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

message(STATUS "[Module] RHI stub registered.")
