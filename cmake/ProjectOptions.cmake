# Warnings and sanitizers, applied only to project targets (never to SDL).
add_library(cs_project_options INTERFACE)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cs_project_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
        -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wdouble-promotion)
    if(CS_WARNINGS_AS_ERRORS)
        target_compile_options(cs_project_options INTERFACE -Werror)
    endif()
elseif(MSVC)
    target_compile_options(cs_project_options INTERFACE /W4)
    if(CS_WARNINGS_AS_ERRORS)
        target_compile_options(cs_project_options INTERFACE /WX)
    endif()
endif()

if(CS_SANITIZERS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(cs_project_options INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(cs_project_options INTERFACE
            -fsanitize=address,undefined)
    else()
        message(WARNING "CS_SANITIZERS is only supported with GCC/Clang")
    endif()
endif()
