add_library(RiftCore_ECS SHARED
    src/World.cpp
    src/ECSModule.cpp
)

target_compile_features(RiftCore_ECS
    PUBLIC cxx_std_17
)

target_include_directories(RiftCore_ECS
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(RiftCore_ECS
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_ECS
    PRIVATE ECS_EXPORTS
)

set_target_properties(RiftCore_ECS PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[Module] ECS registered.")
