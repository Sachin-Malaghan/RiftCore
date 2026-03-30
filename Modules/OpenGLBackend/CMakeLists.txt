add_library(RiftCore_OpenGLBackend SHARED
    src/GLDevice.cpp
    src/OpenGLModule.cpp
)

target_compile_features(RiftCore_OpenGLBackend
    PUBLIC cxx_std_17
)

target_include_directories(RiftCore_OpenGLBackend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(RiftCore_OpenGLBackend
    PUBLIC RiftCore_SDK
)

target_compile_definitions(RiftCore_OpenGLBackend
    PRIVATE OPENGLBACKEND_EXPORTS
)

# Find GLFW and GLAD via vcpkg
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(RiftCore_OpenGLBackend
    PRIVATE glfw
    PRIVATE glad::glad
    PRIVATE OpenGL::GL
)

set_target_properties(RiftCore_OpenGLBackend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[Module] OpenGLBackend registered.")
