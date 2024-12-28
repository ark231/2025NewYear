#include "simple_logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
static LogLevel current_level = ERROR;

#define STR_LOG_CASE(lvl) \
    case lvl:             \
        return #lvl;

const char *stringify_level(LogLevel level) {
    switch (level) {
        STR_LOG_CASE(DEBUG)
        STR_LOG_CASE(INFO)
        STR_LOG_CASE(WARNING)
        STR_LOG_CASE(ERROR)
        STR_LOG_CASE(FATAL)
    }
}

void set_log_level(LogLevel level) { current_level = level; }

void simple_log_impl(LogLevel level, const char *file, int line, const char *func, const char *format, ...) {
    if (level < current_level) {
        return;
    }
    va_list ap;
    va_start(ap, format);
    time_t now;
    time(&now);
    struct tm *p = localtime(&now);
    char timebuf[256];
    strftime(timebuf, sizeof(timebuf), "%Y/%m/%d %H:%M:%S", p);

    const char *fname = file;
    while (*file != '\0') {
        if (*file == '/') {
            fname = file + 1;
        }
        file++;
    }

    printf("%s %s:%d %s [%s]: ", timebuf, fname, line, func, stringify_level(level));
    vprintf(format, ap);
    printf("\n");
    va_end(ap);
}
