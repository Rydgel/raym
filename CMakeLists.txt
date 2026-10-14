cmake_minimum_required(VERSION 3.10)
project(marching_cubes)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Fetch and build raylib
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
    # Pinned to the master commit the web build was verified with, so a
    # moving master can't break the build out from under the site.
    GIT_TAG "f2aa72f8805f029949ac4fb6fa5784e9b8157bc7"
    GIT_PROGRESS TRUE
)

# Web build: `emcmake cmake -B build-web && cmake --build build-web`.
# raylib targets WebGL 2 (GLSL ES 3.00); see LoadShaderPortable in render.c.
if (EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "" FORCE)
    set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
endif()

FetchContent_MakeAvailable(raylib)

# Add source files
set(SOURCES 
    src/main.c
    src/terrain.c
    src/render.c
    src/marching_cubes.c
)

# Add header files
set(HEADERS
    src/chunk.h
    src/marching_cubes.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link raylib
target_link_libraries(${PROJECT_NAME} raylib)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE src)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources)

if (EMSCRIPTEN)
    # index.html + .js + .wasm + .data: serve the folder from any static host.
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "index" SUFFIX ".html")
    target_link_options(${PROJECT_NAME} PRIVATE
        -sUSE_GLFW=3
        -sMIN_WEBGL_VERSION=2
        -sMAX_WEBGL_VERSION=2
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=134217728
        -sSTACK_SIZE=4194304
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/resources/shaders@resources/shaders"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/resources/textures@resources/textures"
        "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html")
    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/web/shell.html)
endif()
