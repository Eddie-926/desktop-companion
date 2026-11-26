
cmake_minimum_required(VERSION 3.20)

project(DesktopCompanion
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.20
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL3)

# SDL3_image
FetchContent_Declare(
    SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.4.8
    GIT_SHALLOW TRUE
)

# Disable AVIF support for now (avoids the dav1d/NASM dependency)
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3_image)

# Main application
add_executable(DesktopCompanion
    src/main.cpp
)

target_link_libraries(DesktopCompanion
    PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
)

# Direct2D prototype
add_executable(Direct2DPrototype
    src/prototypes/direct2d/main.cpp
)

target_link_libraries(Direct2DPrototype
    PRIVATE
        d2d1
        windowscodecs
        ole32
        dcomp
        dxgi
        d3d11
) 