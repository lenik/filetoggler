#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Logger instance structure
typedef struct {
    int level;
    int level_default;
    const char* header;
    const char* color_error;
    const char* color_warn;
    const char* color_mesg;
    const char* color_info;
    const char* color_log;
    const char* color_debug;
    const char* color_trace;
    const char* color_reset;
} logger_t;

// Logger functions that operate on a logger instance
void logger_set_level(logger_t* logger, int value);
int logger_get_level(const logger_t* logger);
void logger_more(logger_t* logger);
void logger_less(logger_t* logger);
void logger_reset(logger_t* logger);

// Logging functions (without format specifiers)
void logger_error(const logger_t* logger, const char* message);
void logger_warn(const logger_t* logger, const char* message);
void logger_mesg(const logger_t* logger, const char* message);
void logger_info(const logger_t* logger, const char* message);
void logger_log(const logger_t* logger, const char* message);
void logger_debug(const logger_t* logger, const char* message);
void logger_trace(const logger_t* logger, const char* message);

// Format versions (with format specifiers)
void logger_error_fmt(const logger_t* logger, const char* format, ...);
void logger_warn_fmt(const logger_t* logger, const char* format, ...);
void logger_mesg_fmt(const logger_t* logger, const char* format, ...);
void logger_info_fmt(const logger_t* logger, const char* format, ...);
void logger_log_fmt(const logger_t* logger, const char* format, ...);
void logger_debug_fmt(const logger_t* logger, const char* format, ...);
void logger_trace_fmt(const logger_t* logger, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif // LOGGER_H

