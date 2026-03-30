add_library(RiftCore_JobSystem SHARED
    src/ThreadPool.cpp
    src/JobSystem.cpp
)

target_compile_features(RiftCore_JobSystem
    PUBLIC cxx_std_17
)

target_include_directories(RiftCore_JobSystem
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(RiftCore_JobSystem
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_JobSystem
    PRIVATE JOBSYSTEM_EXPORTS
)

# Link threading library
find_package(Threads REQUIRED)
target_link_libraries(RiftCore_JobSystem
    PRIVATE Threads::Threads
)

set_target_properties(RiftCore_JobSystem PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[Module] JobSystem registered.")
