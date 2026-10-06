# RiftCore Editor
#
# The editor is src/main.cpp + src/App/*. The older UI under src/UI and
# include/UI (HUD, panels) is kept for reference only and is not compiled.

add_executable(RiftCoreEditor
    src/main.cpp
    src/App/EditorApp.cpp
    src/App/EditorApp.h
    src/App/EditorStyle.cpp
    src/App/HouseDesigner.cpp
    src/App/EditorStyle.h
)

target_compile_features(RiftCoreEditor PRIVATE cxx_std_17)

target_include_directories(RiftCoreEditor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/Modules/Core/include
        ${CMAKE_SOURCE_DIR}/Modules/OpenGLBackend/include
        ${CMAKE_SOURCE_DIR}/Modules/Renderer/include
        ${CMAKE_SOURCE_DIR}/Modules/Physics/include
        ${CMAKE_SOURCE_DIR}/Modules/Scene/include
)

find_package(OpenGL REQUIRED)

target_link_libraries(RiftCoreEditor
    PRIVATE RiftCore_SDK
    PRIVATE RiftCore_Core
    PRIVATE RiftCore_OpenGLBackend
    PRIVATE RiftCore_Renderer
    PRIVATE RiftCore_Physics
    PRIVATE RiftCore_Scene
    PRIVATE imgui
    PRIVATE imguizmo
    PRIVATE nlohmann_json
    PRIVATE glad::glad
    PRIVATE glfw_api
    PRIVATE OpenGL::GL
)
if(WIN32)
    target_link_libraries(RiftCoreEditor PRIVATE comdlg32 shell32)
endif()

# Loaded at run time (not linked): make sure it is built with the editor.
add_dependencies(RiftCoreEditor RiftCore_Scripting)

if(MSVC)
    target_compile_options(RiftCoreEditor PRIVATE /W3)
    target_compile_definitions(RiftCoreEditor PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(RiftCoreEditor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

message(STATUS "[App] RiftCore Editor registered.")
