add_executable(RiftCoreEditor
    src/main.cpp
)

target_link_libraries(RiftCoreEditor
    PRIVATE RiftCore_SDK
)

set_target_properties(RiftCoreEditor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[App] Editor registered.")
