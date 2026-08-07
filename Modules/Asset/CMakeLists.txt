# Asset Module - Stub placeholder
# This file will be replaced when we implement this module



add_library(RiftCore_Asset SHARED
    src/placeholder.cpp
)

target_link_libraries(RiftCore_Asset
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_Asset
    PRIVATE ASSET_EXPORTS
)

target_include_directories(RiftCore_Asset
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

message(STATUS "[Module] Asset stub registered.")
