# Unit and integration tests. None of them opens a real window.
#  - Most link only SDL-free libraries (cs_game / cs_app).
#  - test_platform links SDL but only initializes the events subsystem.
#  - The smoke tests run the real executable headless, or windowed under SDL's "dummy" video driver.

function(cs_test name)
    add_executable(test_${name} test_${name}.c)
    target_link_libraries(test_${name} PRIVATE ${ARGN})
    target_include_directories(test_${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(test_${name} PRIVATE CS_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
    add_test(NAME ${name} COMMAND test_${name} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

cs_test(core cs_game)
cs_test(png cs_game)
cs_test(level cs_game)
cs_test(entity cs_game)
cs_test(physics cs_game)
cs_test(companion cs_game)
cs_test(script cs_game)
cs_test(camera cs_app)
cs_test(game cs_app)
cs_test(crate cs_app)
cs_test(pad cs_app)
cs_test(pad_menu cs_app)
cs_test(events cs_app)
cs_test(scenarios cs_app)
cs_test(platform cs_platform cs_game)

# --- Smoke tests on the real executable ----------------------------------------------------------
set(SHOTS ${CMAKE_BINARY_DIR}/screenshots)
file(MAKE_DIRECTORY ${SHOTS})
set(EXE $<TARGET_FILE:cratesquad>)

add_test(NAME smoke_headless_bot
    COMMAND cratesquad --headless --bot --frames 3000 --seed 7
            --screenshot ${SHOTS}/smoke_bot.png)
add_test(NAME smoke_headless_script_debug
    COMMAND cratesquad --headless --script "R:90 RJ:12 .:60" --debug-overlay
            --screenshot ${SHOTS}/smoke_script_debug.png)
add_test(NAME smoke_headless_filmstrip
    COMMAND cratesquad --headless --bot --frames 600 --seed 3
            --screenshot ${SHOTS}/film_%u.png --shot-every 200)

# Pad: fill it, move the menu selection, validate (L is entry 0, 2x1 vertical entry 1): one product made.
add_test(NAME smoke_pad_build
    COMMAND cratesquad --headless --fill-pad "r...,rr.b" --script ".:5 S:1 .:2 C:1 .:60"
            --screenshot ${SHOTS}/smoke_pad_built.png --shot-cam 0,300)
set_tests_properties(smoke_pad_build PROPERTIES PASS_REGULAR_EXPRESSION "products=1")
# Without validation nothing is built.
add_test(NAME smoke_pad_no_confirm
    COMMAND cratesquad --headless --fill-pad "r...,rr.b" --script ".:5 S:1 .:60")
set_tests_properties(smoke_pad_no_confirm PROPERTIES PASS_REGULAR_EXPRESSION "products=0")

# Bad input must fail cleanly (non-zero exit), never crash.
foreach(case bad_args bad_level bad_script bad_fill_pad)
    if(case STREQUAL "bad_args")
        set(args --nonsense)
    elseif(case STREQUAL "bad_level")
        set(args --headless --level ${CMAKE_SOURCE_DIR}/assets/levels/nope.lvl)
    elseif(case STREQUAL "bad_fill_pad")
        set(args --headless --fill-pad "rrr,rrr")
    else()
        set(args --headless --script "R:abc")
    endif()
    add_test(NAME smoke_${case} COMMAND cratesquad ${args})
    set_tests_properties(smoke_${case} PROPERTIES WILL_FAIL TRUE)
endforeach()

# Same seed twice must print the same final state hash.
add_test(NAME smoke_determinism
    COMMAND ${CMAKE_COMMAND} -DEXE=${EXE} -P ${CMAKE_CURRENT_SOURCE_DIR}/determinism.cmake)

# The real windowed loop (window creation, vsync, FPS title, debug overlay, screenshot), but under
# SDL's "dummy" video driver: no display server involved, no window ever appears.
add_test(NAME smoke_windowed_dummy
    COMMAND ${CMAKE_COMMAND} -DEXE=${EXE} -DSHOT=${SHOTS}/windowed_dummy.png
            -P ${CMAKE_CURRENT_SOURCE_DIR}/windowed_dummy.cmake)

# Safety net: if SDL picked another driver than expected, the app refuses to open a window.
add_test(NAME smoke_driver_guard
    COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy
            ${EXE} --expect-video-driver some-real-driver --max-ticks 5)
set_tests_properties(smoke_driver_guard PROPERTIES
    PASS_REGULAR_EXPRESSION "refusing to open a window")
