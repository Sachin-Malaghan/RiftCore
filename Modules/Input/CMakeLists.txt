add_library(RiftCore_Input SHARED
    src/InputSystem.cpp
)

target_compile_features(RiftCore_Input
    PUBLIC cxx_std_17
)

target_include_directories(RiftCore_Input
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(RiftCore_Input
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_Input
    PRIVATE INPUT_EXPORTS
)

# Input needs GLFW and GLAD (shares window with OpenGLBackend)
find_package(glfw3 CONFIG REQUIRED)
find_package(glad  CONFIG REQUIRED)

target_link_libraries(RiftCore_Input
    PRIVATE glfw
    PRIVATE glad::glad
)

set_target_properties(RiftCore_Input PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[Module] Input registered.")
