#ifndef __LOG_HPP__
#define __LOG_HPP__

#include <cstdio>
#include <cstdarg>  // Required for va_list

namespace robust_serial
{

/**
 * @brief Logging levels for debugging.
 */
enum LogLevel {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_DEBUG
};

/**
 * @brief Lightweight logging function.
 * @param level Log level (INFO, WARNING, ERROR, DEBUG).
 * @param format Message format (printf-style).
 * @param ... Additional arguments.
 */
inline void log(LogLevel level, const char* format, ...)
{
    static const char* level_str[] = {"[INFO] ", "[WARNING] ", "[ERROR] ", "[DEBUG] "};

    printf("%s", level_str[level]);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

/** @brief Log an informational message. */
#define log_info(format, ...) log(LOG_LEVEL_INFO, format, ##__VA_ARGS__)

/** @brief Log a warning message. */
#define log_warning(format, ...) log(LOG_LEVEL_WARNING, format, ##__VA_ARGS__)

/** @brief Log an error message. */
#define log_error(format, ...) log(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

/** @brief Log a debug message. */
#define log_debug(format, ...) log(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)

} // namespace robust_serial

#endif // __LOG_HPP__
