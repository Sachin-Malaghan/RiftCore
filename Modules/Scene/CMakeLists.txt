add_library(RiftCore_Scene SHARED
    src/SceneNode.cpp
    src/SceneSystem.cpp
)

target_compile_features(RiftCore_Scene
    PUBLIC cxx_std_17
)

target_include_directories(RiftCore_Scene
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/ThirdParty/json
)

target_link_libraries(RiftCore_Scene
    PUBLIC  RiftCore_SDK
    PRIVATE nlohmann_json
)

target_compile_definitions(RiftCore_Scene
    PRIVATE SCENE_EXPORTS
)

if(MSVC)
    target_compile_options(RiftCore_Scene PRIVATE /W3)
    target_compile_definitions(RiftCore_Scene
        PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(RiftCore_Scene PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[Module] Scene registered.")
