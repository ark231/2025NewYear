#ifndef SIMPLE_LOGGING
#define SIMPLE_LOGGING

typedef enum {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL,
} LogLevel;

#define DEFAULT_LOG_LEVEL ERROR

void set_log_level(LogLevel level);
void simple_log_impl(LogLevel level, const char* file, int line, const char* func, const char* format, ...);
LogLevel parse_log_level(const char* str);

#define SIMPLE_LOG(level, format, ...) simple_log_impl(level, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#endif
