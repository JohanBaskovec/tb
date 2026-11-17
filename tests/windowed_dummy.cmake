# Runs the interactive loop for a few seconds under SDL's dummy video driver and checks:
#  - it exits cleanly after --max-ticks,
#  - the FPS/TPS counter reached the window title,
#  - the screenshot was written.
# --expect-video-driver dummy makes the app abort before creating a window if SDL picked anything else.
file(REMOVE ${SHOT})
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
            ${EXE} --expect-video-driver dummy --max-ticks 150
            --script "R:100 RJ:10 .:40" --debug-overlay --screenshot ${SHOT}
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "exit code ${rc}\nstdout: ${out}\nstderr: ${err}")
endif()
if(NOT out MATCHES "title=Crate Squad - [0-9]+ FPS / [0-9]+ TPS")
    message(FATAL_ERROR "FPS/TPS title missing from output:\n${out}")
endif()
if(NOT out MATCHES "ticks=150 ")
    message(FATAL_ERROR "did not stop at 150 ticks:\n${out}")
endif()
if(NOT EXISTS ${SHOT})
    message(FATAL_ERROR "screenshot not written: ${SHOT}")
endif()
message(STATUS "windowed loop OK under the dummy driver: ${out}")
