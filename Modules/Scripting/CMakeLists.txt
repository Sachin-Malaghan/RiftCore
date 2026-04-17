# Modules/Scripting/CMakeLists.txt

# 1. Define the library and its source files
# Replace placeholder.cpp with your actual implementation file once created
add_library(RiftCore_Scripting SHARED
    src/ScriptingModule.cpp
)

# 2. Link against the SDK (This is crucial for IScripting.h to see Result.h, Types.h, etc.)
target_link_libraries(RiftCore_Scripting
    PUBLIC RiftCore_SDK
)

# 3. Export definitions
target_compile_definitions(RiftCore_Scripting
    PRIVATE SCRIPTING_EXPORTS
)

# 4. Include Directories
target_include_directories(RiftCore_Scripting
    PUBLIC 
        # Path to this module's own headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # Path to the SDK headers (resolves <RiftCore/Common/...>)
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/SDK/include> 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 5. Ensure the .lib and .dll are sent to the correct global folders
set_target_properties(RiftCore_Scripting PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "[Module] Scripting registered and SDK paths mapped.")