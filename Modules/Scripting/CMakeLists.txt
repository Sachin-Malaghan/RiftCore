# Scripting Module - Stub placeholder
# This file will be replaced when we implement this module

add_library(RiftCore_Scripting SHARED
    src/placeholder.cpp
)

target_link_libraries(RiftCore_Scripting
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_Scripting
    PRIVATE SCRIPTING_EXPORTS
)

target_include_directories(RiftCore_Scripting
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

message(STATUS "[Module] Scripting stub registered.")
