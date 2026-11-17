#include "core/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static LogLevel g_level = LOG_LEVEL_INFO;

void log_set_level(LogLevel level) {
    g_level = level;
}

void log_write(LogLevel level, const char *file, int line, const char *fmt, ...) {
    static const char *const names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    if (level < g_level || level >= LOG_LEVEL_OFF) {
        return;
    }
    fprintf(stderr, "[%s] %s:%d: ", names[level], file, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

void log_assert_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "[ASSERT] %s:%d: %s\n", file, line, expr);
    fflush(stderr);
    abort();
}
