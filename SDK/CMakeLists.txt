# ============================================================
# SDK — Header-only interface library
# ============================================================
add_library(RiftCore_SDK INTERFACE)


target_include_directories(RiftCore_SDK
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(RiftCore_SDK
    INTERFACE
        cxx_std_17
)

message(STATUS "[SDK] RiftCore SDK interface target created.")
