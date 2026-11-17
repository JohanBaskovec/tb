# Runs the headless game twice with the same seed (and once with another) and compares hashes.
function(run_game seed out_var)
    execute_process(
        COMMAND ${EXE} --headless --bot --frames 5000 --seed ${seed}
        OUTPUT_VARIABLE out
        RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "game exited with ${rc}")
    endif()
    string(REGEX MATCH "hash=[0-9a-f]+" hash "${out}")
    if(hash STREQUAL "")
        message(FATAL_ERROR "no hash in output: ${out}")
    endif()
    set(${out_var} "${hash}" PARENT_SCOPE)
endfunction()

run_game(11 a)
run_game(11 b)
run_game(12 c)
if(NOT a STREQUAL b)
    message(FATAL_ERROR "non-deterministic: ${a} vs ${b}")
endif()
if(a STREQUAL c)
    message(FATAL_ERROR "different seeds gave the same hash: ${a}")
endif()
message(STATUS "deterministic: ${a}")
