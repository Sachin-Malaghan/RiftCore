add_library(RiftCore_Core SHARED
    src/Logger.cpp
    src/EventBus.cpp
    src/MemoryAllocator.cpp
    src/PluginManager.cpp
    src/Engine.cpp
)

target_compile_features(RiftCore_Core
    PUBLIC cxx_std_17
)

target_include_directories(RiftCore_Core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(RiftCore_Core
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_Core
    PRIVATE CORE_EXPORTS
)

set_target_properties(RiftCore_Core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[Module] Core registered.")
