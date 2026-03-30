# ThirdParty libraries

# stb_image - single header image loader
add_library(stb_image STATIC
    stb/stb_image_impl.cpp
)

target_include_directories(stb_image
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/stb
)

# Suppress warnings in third party code
if(MSVC)
    target_compile_options(stb_image
        PRIVATE /W0)
else()
    target_compile_options(stb_image
        PRIVATE -w)
endif()

message(STATUS "[ThirdParty] stb_image configured.")
