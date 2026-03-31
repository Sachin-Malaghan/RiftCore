add_library(RiftCore_Physics SHARED
    src/RigidBody.cpp
    src/PhysicsWorld.cpp
)

target_compile_features(RiftCore_Physics
    PUBLIC cxx_std_17
)

target_include_directories(RiftCore_Physics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(RiftCore_Physics
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_Physics
    PRIVATE PHYSICS_EXPORTS
)

find_package(Threads REQUIRED)
target_link_libraries(RiftCore_Physics
    PRIVATE Threads::Threads
)

set_target_properties(RiftCore_Physics PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[Module] Physics registered.")
