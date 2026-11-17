// Leveled logging to stderr and an ASSERT macro (active unless NDEBUG).
#pragma once

#include "core/types.h"

typedef enum LogLevel {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_OFF
} LogLevel;

#if defined(__GNUC__) || defined(__clang__)
#define CS_PRINTF_LIKE(fmt_idx, first_arg) __attribute__((format(printf, fmt_idx, first_arg)))
#else
#define CS_PRINTF_LIKE(fmt_idx, first_arg)
#endif

void log_set_level(LogLevel level);
void log_write(LogLevel level, const char *file, int line, const char *fmt, ...)
    CS_PRINTF_LIKE(4, 5);
void log_assert_fail(const char *expr, const char *file, int line);

#define LOG_DEBUG(...) log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) log_write(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#ifndef NDEBUG
#define ASSERT(cond)                                                                               \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            log_assert_fail(#cond, __FILE__, __LINE__);                                            \
        }                                                                                          \
    } while (0)
#else
#define ASSERT(cond) ((void)0)
#endif
