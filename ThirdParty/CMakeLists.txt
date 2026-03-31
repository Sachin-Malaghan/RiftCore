# ── stb_image ─────────────────────────────────────────────
add_library(stb_image STATIC
    stb/stb_image_impl.cpp
)
target_include_directories(stb_image PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/stb)
if(MSVC)
    target_compile_options(stb_image PRIVATE /W0)
else()
    target_compile_options(stb_image PRIVATE -w)
endif()

# ── miniaudio ──────────────────────────────────────────────
add_library(miniaudio STATIC
    miniaudio/miniaudio_impl.cpp
)
target_include_directories(miniaudio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio)
if(WIN32)
    target_link_libraries(miniaudio PUBLIC ole32 winmm)
endif()
if(MSVC)
    target_compile_options(miniaudio PRIVATE /W0)
    target_compile_definitions(miniaudio
        PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(miniaudio PRIVATE -w)
endif()

# ── nlohmann/json ─────────────────────────────────────────
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/json)

# ── ImGuizmo ──────────────────────────────────────────────
find_package(imgui CONFIG REQUIRED)

add_library(imguizmo STATIC
    imguizmo/ImGuizmo.cpp
)
target_include_directories(imguizmo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imguizmo)
target_link_libraries(imguizmo PUBLIC imgui::imgui)
if(MSVC)
    target_compile_options(imguizmo PRIVATE /W0)
endif()

message(STATUS "[ThirdParty] All libraries configured.")
