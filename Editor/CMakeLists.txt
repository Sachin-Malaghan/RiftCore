cmake_minimum_required(VERSION 3.20)
project(RiftCoreEditor)

# ── 1. Source Discovery ────────────────────────────
# Find all our Editor UI files (this will automatically grab src/UI/HUD.cpp now!)
file(GLOB_RECURSE UI_SOURCES 
    "src/UI/*.cpp"
    "src/UI/Panels/*.cpp"
    "src/UI/Styling/*.cpp"
    "src/UI/Commands/*.cpp"
)

# Find ALL ImGui Base Files
file(GLOB IMGUI_SOURCES
    "../ThirdParty/imgui/*.cpp"
)

# ImGui Backend Files (GLFW + OpenGL3)
set(IMGUI_BACKEND_SOURCES
    "../ThirdParty/imgui/backends/imgui_impl_glfw.cpp"
    "../ThirdParty/imgui/backends/imgui_impl_opengl3.cpp"
)

# Node Editor Files
set(NODE_EDITOR_SOURCES
    "../ThirdParty/imgui-node-editor/imgui_node_editor.cpp"
    "../ThirdParty/imgui-node-editor/imgui_node_editor_api.cpp"
    "../ThirdParty/imgui-node-editor/imgui_canvas.cpp"
    "../ThirdParty/imgui-node-editor/crude_json.cpp"
)

# ── 2. Register Executable ──────────────────────────────────
add_executable(RiftCoreEditor 
    src/main.cpp 
    src/GizmoSystem.cpp
    ${UI_SOURCES}
    ${IMGUI_SOURCES}
    ${IMGUI_BACKEND_SOURCES}
    ${NODE_EDITOR_SOURCES}
)

target_compile_features(RiftCoreEditor PRIVATE cxx_std_17)

# ── 3. Include Directories ──────────────────────────────────
target_include_directories(RiftCoreEditor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/UI
        ${CMAKE_CURRENT_SOURCE_DIR}/include/UI/Panels
        ${CMAKE_CURRENT_SOURCE_DIR}/include/UI/Styling
        ${CMAKE_CURRENT_SOURCE_DIR}/include/UI/Commands
        ${CMAKE_SOURCE_DIR}/Modules/Core/include
        ${CMAKE_SOURCE_DIR}/Modules/OpenGLBackend/include
        ${CMAKE_SOURCE_DIR}/Modules/Input/include
        ${CMAKE_SOURCE_DIR}/Modules/Renderer/include
        ${CMAKE_SOURCE_DIR}/Modules/Physics/include
        ${CMAKE_SOURCE_DIR}/Modules/Scene/include
        ${CMAKE_SOURCE_DIR}/ThirdParty/imguizmo
        ${CMAKE_SOURCE_DIR}/ThirdParty/imgui
        ${CMAKE_SOURCE_DIR}/ThirdParty/imgui/backends
        ${CMAKE_SOURCE_DIR}/ThirdParty/imgui-node-editor
        ${CMAKE_SOURCE_DIR}/ThirdParty/stb
)

# ── 4. Dependencies ─────────────────────────────────────────
find_package(glad  CONFIG REQUIRED)
# Added glfw3 and OpenGL because the Editor now handles ImGui window backends directly
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(RiftCoreEditor
    PRIVATE RiftCore_SDK
    PRIVATE RiftCore_Core
    PRIVATE RiftCore_OpenGLBackend
    PRIVATE RiftCore_Input
    PRIVATE RiftCore_Renderer
    PRIVATE RiftCore_Physics
    PRIVATE RiftCore_Scene
    PRIVATE imguizmo
    PRIVATE glad::glad
    PRIVATE glfw
    PRIVATE OpenGL::GL
)

# ── 5. Compiler Settings ────────────────────────────────────
if(MSVC)
    target_compile_options(RiftCoreEditor PRIVATE /W3)
    target_compile_definitions(RiftCoreEditor PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ── 6. Binary Output Handling ───────────────────────────────
set_target_properties(RiftCoreEditor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[App] Modular RiftCore Editor registered (Local ImGui Docking Branch).")
