# ============================================================
# ThirdParty - every dependency is vendored in this folder, so a
# fresh clone builds with nothing but CMake + a C++17 compiler.
# ============================================================

# -- GLFW 3.4 (shared: one window/context state for all modules)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(_rc_shared_backup ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS ON)
add_subdirectory(glfw)
set(BUILD_SHARED_LIBS ${_rc_shared_backup})
set_target_properties(glfw PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")

# -- glad (OpenGL 4.6 loader) ---------------------------------
add_library(glad STATIC glad/src/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/include)
set_target_properties(glad PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(glad::glad ALIAS glad)

# -- stb_image -------------------------------------------------
add_library(stb_image STATIC stb/stb_image_impl.cpp)
target_include_directories(stb_image PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb)
set_target_properties(stb_image PROPERTIES POSITION_INDEPENDENT_CODE ON)

# -- miniaudio -------------------------------------------------
add_library(miniaudio STATIC miniaudio/miniaudio_impl.cpp)
target_include_directories(miniaudio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio)
set_target_properties(miniaudio PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(WIN32)
    target_link_libraries(miniaudio PUBLIC ole32 winmm)
endif()

# -- nlohmann/json ---------------------------------------------
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/json)

# -- Dear ImGui 1.92.6 + GLFW/OpenGL3 backends -----------------
find_package(OpenGL REQUIRED)
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/misc/cpp/imgui_stdlib.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/cpp)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
add_library(imgui::imgui ALIAS imgui)

# -- ImGuizmo --------------------------------------------------
add_library(imguizmo STATIC imguizmo/ImGuizmo.cpp)
target_include_directories(imguizmo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imguizmo)
target_link_libraries(imguizmo PUBLIC imgui)

foreach(_t glad stb_image miniaudio imgui imguizmo)
    if(MSVC)
        target_compile_options(${_t} PRIVATE /W0)
        target_compile_definitions(${_t} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${_t} PRIVATE -w)
    endif()
endforeach()

message(STATUS "[ThirdParty] All libraries configured (vendored).")
