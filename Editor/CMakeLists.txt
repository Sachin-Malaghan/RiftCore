add_executable(RiftCoreEditor
    src/main.cpp
    src/EditorUI.cpp
    src/GizmoSystem.cpp
)

target_compile_features(RiftCoreEditor
    PRIVATE cxx_std_17
)

target_include_directories(RiftCoreEditor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/Modules/Core/include
        ${CMAKE_SOURCE_DIR}/Modules/OpenGLBackend/include
        ${CMAKE_SOURCE_DIR}/Modules/Input/include
        ${CMAKE_SOURCE_DIR}/Modules/Renderer/include
        ${CMAKE_SOURCE_DIR}/Modules/Physics/include
        ${CMAKE_SOURCE_DIR}/Modules/Scene/include
        ${CMAKE_SOURCE_DIR}/ThirdParty/imguizmo
)

target_link_libraries(RiftCoreEditor
    PRIVATE RiftCore_SDK
    PRIVATE RiftCore_Core
    PRIVATE RiftCore_OpenGLBackend
    PRIVATE RiftCore_Input
    PRIVATE RiftCore_Renderer
    PRIVATE RiftCore_Physics
    PRIVATE RiftCore_Scene
    PRIVATE imguizmo
)

find_package(glad   CONFIG REQUIRED)
find_package(imgui  CONFIG REQUIRED)
target_link_libraries(RiftCoreEditor
    PRIVATE glad::glad
    PRIVATE imgui::imgui
)

if(MSVC)
    target_compile_options(RiftCoreEditor
        PRIVATE /W3)
    target_compile_definitions(RiftCoreEditor
        PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(RiftCoreEditor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[App] Editor registered.")
