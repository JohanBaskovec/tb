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
cs_test(debug_menu cs_app)
cs_test(goal cs_app)
cs_test(events cs_app)
cs_test(combat cs_app)
cs_test(squad cs_app)
cs_test(nav cs_game)
cs_test(ai cs_app)
cs_test(rules cs_app)
cs_test(front cs_app)
cs_test(sfx cs_app)
cs_test(music cs_app)
cs_test(sprites cs_app)
cs_test(fx cs_app)
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

# Combat: an uzi burst empties a dummy of the arena (100 HP / 6 per bullet); a rocket explodes in the picture.
add_test(NAME smoke_uzi_kills_a_dummy
    COMMAND cratesquad --headless --give uzi --start 440,432 --script "R:3 F:90 .:30"
            --screenshot ${SHOTS}/smoke_uzi.png --shot-cam 350,330)
set_tests_properties(smoke_uzi_kills_a_dummy PROPERTIES PASS_REGULAR_EXPRESSION "kills=1 ")
add_test(NAME smoke_rocket_explosion
    COMMAND cratesquad --headless --give rocket --start 400,432 --script "R:3 .:5 F:1 .:27"
            --screenshot ${SHOTS}/smoke_rocket.png --shot-cam 350,330)
set_tests_properties(smoke_rocket_explosion PROPERTIES PASS_REGULAR_EXPRESSION "shots=1 hits=1 ")
# Aiming: a pistol aimed (script aim, like the mouse) at the dummy's chest hits it; aimed at the sky it does not.
add_test(NAME smoke_aimed_shot_hits
    COMMAND cratesquad --headless --give pistol --start 400,432 --script ".:3 F@488/420:1 .:60"
            --screenshot ${SHOTS}/smoke_aim.png --shot-cam 350,330)
set_tests_properties(smoke_aimed_shot_hits PROPERTIES PASS_REGULAR_EXPRESSION "shots=1 hits=1 ")
add_test(NAME smoke_aimed_shot_misses
    COMMAND cratesquad --headless --give pistol --start 400,432 --script ".:3 F@488/300:1 .:60")
set_tests_properties(smoke_aimed_shot_misses PROPERTIES PASS_REGULAR_EXPRESSION "shots=1 hits=0 ")
# Squads: followers keep up with the leader; the screenshot shows the variants; an AI-heavy bot game stays sane.
add_test(NAME smoke_squad_follows
    COMMAND cratesquad --headless --ai-team none --soldier 1,strong,70 --soldier 1,heavy,90 --frames 600
            --screenshot ${SHOTS}/smoke_squad.png --shot-cam 0,330)
set_tests_properties(smoke_squad_follows PROPERTIES PASS_REGULAR_EXPRESSION "soldiers=3/0 order=follow ")
add_test(NAME smoke_squad_bot_vs_ai
    COMMAND cratesquad --headless --bot --soldier 1,heavy,100 --soldier 2,heavy,600 --soldier 2,strong,640 --frames 4000 --seed 9)
add_test(NAME smoke_armed_bot
    COMMAND cratesquad --headless --bot --give uzi --frames 3000 --seed 4)

# AI: two AI teams play on the symmetric level: both gather crates, build and grow (more than one soldier each);
# the debug overlay draws the navigation graph and the paths. A human against the default AI opponent also works.
set(AI_ARENA ${CMAKE_SOURCE_DIR}/assets/levels/ai_arena.lvl)
add_test(NAME smoke_ai_match
    COMMAND cratesquad --headless --level ${AI_ARENA} --ai-team 1,2 --frames 6000 --seed 2 --debug-nav
            --screenshot ${SHOTS}/smoke_ai_match.png --shot-cam 380,300)
set_tests_properties(smoke_ai_match PROPERTIES PASS_REGULAR_EXPRESSION "products=[1-9][0-9]* .*squads=[0-9]+/[0-9]+/0/0")
add_test(NAME smoke_ai_opponent_vs_bot
    COMMAND cratesquad --headless --level ${AI_ARENA} --bot --frames 5000 --seed 5)
set_tests_properties(smoke_ai_opponent_vs_bot PROPERTIES PASS_REGULAR_EXPRESSION "squads=[0-9]+/[1-9][0-9]*/0/0")
add_test(NAME smoke_ai_on_the_test_arena
    COMMAND cratesquad --headless --ai-team 1,2 --frames 4000 --seed 3)

# Menus: the title screen; the way from it to a match and its pause menu; quitting from the title screen.
add_test(NAME smoke_front_title
    COMMAND cratesquad --headless --front --frames 90 --screenshot ${SHOTS}/front_title.png)
set_tests_properties(smoke_front_title PROPERTIES PASS_REGULAR_EXPRESSION "screen=title ")
add_test(NAME smoke_front_setup
    COMMAND cratesquad --headless --front --script ".:3 C:1 .:20" --screenshot ${SHOTS}/front_setup.png)
set_tests_properties(smoke_front_setup PROPERTIES PASS_REGULAR_EXPRESSION "screen=setup level=beach.lvl")
add_test(NAME smoke_front_to_pause
    COMMAND cratesquad --headless --front --script ".:3 C:1 .:3 C:1 R:90 P:1 .:5" --screenshot ${SHOTS}/front_pause.png)
set_tests_properties(smoke_front_to_pause PROPERTIES PASS_REGULAR_EXPRESSION "screen=pause .*ticks=9[0-9] ")
add_test(NAME smoke_front_quit
    COMMAND cratesquad --headless --front --script ".:2 D:1 .:1 C:1 .:5")
set_tests_properties(smoke_front_quit PROPERTIES PASS_REGULAR_EXPRESSION "screen=quit")
add_test(NAME smoke_front_windowed_dummy
    COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
            ${EXE} --expect-video-driver dummy --max-ticks 200 --script ".:3 C:1 .:3 C:1 R:150"
            --screenshot ${SHOTS}/front_windowed.png)
set_tests_properties(smoke_front_windowed_dummy PROPERTIES PASS_REGULAR_EXPRESSION "audio=dummy music=off.*screen=play")

# The music is off until M is pressed; --music starts with it on; --no-music has no music at all.
add_test(NAME smoke_music_flag
    COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
            ${EXE} --expect-video-driver dummy --max-ticks 30 --music --script ".:3 C:1 .:3 C:1 R:20")
set_tests_properties(smoke_music_flag PROPERTIES PASS_REGULAR_EXPRESSION "audio=dummy music=on")
add_test(NAME smoke_no_music_flag
    COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
            ${EXE} --expect-video-driver dummy --max-ticks 30 --no-music --music --script ".:3 C:1 .:3 C:1 R:20")
set_tests_properties(smoke_no_music_flag PROPERTIES PASS_REGULAR_EXPRESSION "audio=dummy music=off")

# The build goal: an ally brings the two crates of a 2x1 horizontal shape to the pad and builds it (team 1 shows one build).
add_test(NAME smoke_goal_builds
    COMMAND cratesquad --headless --goal 2x1h,red --order collect --soldier 1,normal,120 --crate 150,300,red --crate 170,300,red
            --crate 200,300,red --frames 2400 --ai-team none --screenshot ${SHOTS}/goal_built.png)
set_tests_properties(smoke_goal_builds PROPERTIES PASS_REGULAR_EXPRESSION "stats=1:0:0/")
add_test(NAME smoke_goal_menu
    COMMAND cratesquad --headless --goal-menu --goal 4x2,blue --frames 30 --screenshot ${SHOTS}/goal_menu.png)
set_tests_properties(smoke_goal_menu PROPERTIES PASS_REGULAR_EXPRESSION "ticks=30 ")
add_test(NAME bad_goal_option COMMAND cratesquad --headless --goal 3x3,red)
set_tests_properties(bad_goal_option PROPERTIES WILL_FAIL TRUE)

# The F2 spawn menu is drawn in screenshots with --debug-menu.
add_test(NAME smoke_debug_menu
    COMMAND cratesquad --headless --debug-menu --frames 30 --screenshot ${SHOTS}/debug_menu.png)
set_tests_properties(smoke_debug_menu PROPERTIES PASS_REGULAR_EXPRESSION "ticks=30 ")

add_test(NAME smoke_sprite_sheet COMMAND cratesquad --sprite-sheet ${SHOTS}/sprite_sheet.png)
set_tests_properties(smoke_sprite_sheet PROPERTIES PASS_REGULAR_EXPRESSION "sprite sheet: [0-9]+ sprites x 5 palettes")
file(MAKE_DIRECTORY ${SHOTS}/audio)
add_test(NAME smoke_export_audio COMMAND cratesquad --export-audio ${SHOTS}/audio)
set_tests_properties(smoke_export_audio PROPERTIES PASS_REGULAR_EXPRESSION "exported [0-9]+ sounds and 4 songs")

# Bad input must fail cleanly (non-zero exit), never crash.
foreach(case bad_args bad_level bad_script bad_fill_pad bad_give bad_aim bad_soldier bad_order bad_ai_team bad_export_dir)
    if(case STREQUAL "bad_args")
        set(args --nonsense)
    elseif(case STREQUAL "bad_level")
        set(args --headless --level ${CMAKE_SOURCE_DIR}/assets/levels/nope.lvl)
    elseif(case STREQUAL "bad_soldier")
        set(args --headless --soldier 1,tank,100)
    elseif(case STREQUAL "bad_export_dir")
        set(args --export-audio ${CMAKE_BINARY_DIR}/no/such/dir)
    elseif(case STREQUAL "bad_ai_team")
        set(args --headless --ai-team 9)
    elseif(case STREQUAL "bad_order")
        set(args --headless --order dance)
    elseif(case STREQUAL "bad_aim")
        set(args --headless --script "F@12:1")
    elseif(case STREQUAL "bad_give")
        set(args --headless --give bazooka)
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
