add_executable(RiftCoreRuntime
    src/main.cpp
)

target_compile_features(RiftCoreRuntime PRIVATE cxx_std_17)

target_link_libraries(RiftCoreRuntime
    PRIVATE RiftCore_SDK
    PRIVATE RiftCore_Core
    PRIVATE RiftCore_OpenGLBackend
    PRIVATE RiftCore_Input
    PRIVATE RiftCore_Renderer
    PRIVATE RiftCore_Physics
    PRIVATE RiftCore_Scene
    PRIVATE glad::glad
    PRIVATE glfw
)

# Loaded at run time (not linked): make sure they are built with the app.
add_dependencies(RiftCoreRuntime RiftCore_Scripting)

target_include_directories(RiftCoreRuntime
    PRIVATE
        ${CMAKE_SOURCE_DIR}/SDK/include
        ${CMAKE_SOURCE_DIR}/Modules/Core/include
        ${CMAKE_SOURCE_DIR}/Modules/OpenGLBackend/include
        ${CMAKE_SOURCE_DIR}/Modules/Input/include
        ${CMAKE_SOURCE_DIR}/Modules/Renderer/include
        ${CMAKE_SOURCE_DIR}/Modules/Physics/include
        ${CMAKE_SOURCE_DIR}/Modules/Scene/include
)

if(MSVC)
    target_compile_options(RiftCoreRuntime PRIVATE /W3)
    target_compile_definitions(RiftCoreRuntime PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(RiftCoreRuntime PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[App] Runtime registered.")
