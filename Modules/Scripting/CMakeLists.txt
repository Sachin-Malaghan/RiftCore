# Modules/Scripting - embedded Python (optional: without a Python
# development install the module still builds, with scripting disabled).

add_library(RiftCore_Scripting SHARED
    src/ScriptingModule.cpp
)

target_compile_features(RiftCore_Scripting PUBLIC cxx_std_17)

target_link_libraries(RiftCore_Scripting PUBLIC RiftCore_SDK)

target_compile_definitions(RiftCore_Scripting PRIVATE SCRIPTING_EXPORTS)

target_include_directories(RiftCore_Scripting
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/SDK/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

option(RIFTCORE_ENABLE_PYTHON "Embed Python for scripting" ON)

if(RIFTCORE_ENABLE_PYTHON)
    find_package(Python3 COMPONENTS Interpreter Development.Embed QUIET)
endif()

if(RIFTCORE_ENABLE_PYTHON AND Python3_Development.Embed_FOUND)
    # Link the release import library in every configuration: the
    # python.org installer ships no debug build by default.
    target_include_directories(RiftCore_Scripting PRIVATE ${Python3_INCLUDE_DIRS})
    if(WIN32)
        target_link_directories(RiftCore_Scripting PRIVATE ${Python3_LIBRARY_DIRS})
    else()
        target_link_libraries(RiftCore_Scripting PRIVATE ${Python3_LIBRARIES})
    endif()

    get_filename_component(_rc_py_home "${Python3_EXECUTABLE}" DIRECTORY)
    file(TO_CMAKE_PATH "${_rc_py_home}" _rc_py_home)
    target_compile_definitions(RiftCore_Scripting PRIVATE
        RIFTCORE_WITH_PYTHON
        RIFTCORE_PYTHON_HOME="${_rc_py_home}"
        RIFTCORE_PYTHON_VERSION="${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")

    # Put the Python runtime next to the executables so the module loads
    # even when Python is not on PATH.
    if(WIN32)
        set(_rc_py_dll "${_rc_py_home}/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll")
        if(EXISTS "${_rc_py_dll}")
            add_custom_command(TARGET RiftCore_Scripting POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_rc_py_dll}" "${CMAKE_BINARY_DIR}/bin"
                VERBATIM)
        endif()
    endif()
    message(STATUS "[Module] Scripting registered with Python ${Python3_VERSION} (${_rc_py_home}).")
else()
    message(STATUS "[Module] Scripting registered WITHOUT Python - install Python 3 "
                   "(with development files) and re-run CMake to enable scripting.")
endif()

set_target_properties(RiftCore_Scripting PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)
