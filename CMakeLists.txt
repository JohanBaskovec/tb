cmake_minimum_required(VERSION 3.24)
project(CrateSquad VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(CS_USE_SYSTEM_SDL "Use the system SDL3 instead of downloading it" OFF)
option(CS_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(CS_SANITIZERS "Enable ASan + UBSan on project targets" OFF)
option(CS_BUILD_TESTS "Build unit tests" ON)

include(cmake/ProjectOptions.cmake)
include(cmake/FetchSDL.cmake)

# --- Libraries without SDL (testable headless) -------------------------------
add_library(cs_core STATIC
    src/core/log.c
    src/core/arena.c
    src/core/rng.c
    src/core/png.c
)
target_include_directories(cs_core PUBLIC src)
target_link_libraries(cs_core PUBLIC cs_project_options)

add_library(cs_game STATIC
    src/game/game.c
    src/game/entity.c
    src/game/crate.c
    src/game/pad.c
    src/game/level.c
    src/game/physics.c
    src/game/companion.c
    src/game/script.c
    src/game/walker.c
    src/game/bot.c
)
target_link_libraries(cs_game PUBLIC cs_core)
if(UNIX)
    target_link_libraries(cs_game PUBLIC m)
endif()

# Camera and the headless-capable simulation wrapper (still no SDL).
add_library(cs_app STATIC
    src/render/camera.c
    src/app/sim.c
    src/app/pad_menu.c
)
target_link_libraries(cs_app PUBLIC cs_game)

# --- Libraries and executable that use SDL -----------------------------------
add_library(cs_platform STATIC src/platform/platform.c)
target_link_libraries(cs_platform PUBLIC cs_core cs_sdl)

add_executable(cratesquad
    src/main.c
    src/render/render.c
    src/render/draw_game.c
)
target_link_libraries(cratesquad PRIVATE cs_app cs_platform cs_sdl)
# Development default; override at run time with --assets DIR.
target_compile_definitions(cratesquad PRIVATE CS_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

# --- Tests -------------------------------------------------------------------
if(CS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
