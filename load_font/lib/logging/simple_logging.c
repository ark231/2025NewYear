#include "simple_logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static LogLevel current_level = DEFAULT_LOG_LEVEL;

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

    fprintf(stderr, "%s %s:%d %s [%s]: ", timebuf, fname, line, func, stringify_level(level));
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}
#define PARSE_LOG_CASE(str, level)          \
    do {                                    \
        if (strcmp((str), (#level)) == 0) { \
            return level;                   \
        }                                   \
    } while (false)
LogLevel parse_log_level(const char *str) {
    PARSE_LOG_CASE(str, DEBUG);
    PARSE_LOG_CASE(str, INFO);
    PARSE_LOG_CASE(str, WARNING);
    PARSE_LOG_CASE(str, ERROR);
    PARSE_LOG_CASE(str, FATAL);
    return DEFAULT_LOG_LEVEL;
}
