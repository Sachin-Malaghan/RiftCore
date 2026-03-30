add_library(RiftCore_Renderer SHARED
    src/Camera.cpp
    src/MeshFactory.cpp
    src/RenderSystem.cpp
    src/TextureLoader.cpp
    src/OBJLoader.cpp
    src/HUD.cpp
)

target_compile_features(RiftCore_Renderer
    PUBLIC cxx_std_17
)

target_include_directories(RiftCore_Renderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/Modules/OpenGLBackend/include
        ${CMAKE_SOURCE_DIR}/ThirdParty/stb
)

target_link_libraries(RiftCore_Renderer
    PUBLIC  RiftCore_SDK
    PRIVATE RiftCore_OpenGLBackend
    PRIVATE stb_image
)

target_compile_definitions(RiftCore_Renderer
    PRIVATE RENDERER_EXPORTS
)

find_package(glfw3  CONFIG REQUIRED)
find_package(glad   CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui  CONFIG REQUIRED)

target_link_libraries(RiftCore_Renderer
    PRIVATE glfw
    PRIVATE glad::glad
    PRIVATE OpenGL::GL
    PRIVATE imgui::imgui
)

set_target_properties(RiftCore_Renderer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[Module] Renderer registered.")
