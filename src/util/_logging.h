// NOTICE: This header can be included multiple times.

#include "logger.h"

#ifdef __cplusplus
extern "C" {
#endif

// Default to sf_logger if LOGGER is not defined
#ifndef LOGGER
#define LOGGER default_logger
#endif

// Forward declaration of default logger instance
extern logger_t LOGGER;

// Level control macros that use LOGGER instance
#define set_loglevel(value) logger_set_level(&LOGGER, value)
#define get_loglevel() logger_get_level(&LOGGER)
#define log_more() logger_more(&LOGGER)
#define log_less() logger_less(&LOGGER)
#define log_reset() logger_reset(&LOGGER)

// Logging macros that use LOGGER instance
#define logerror(msg) logger_error(&LOGGER, msg)
#define logwarn(msg) logger_warn(&LOGGER, msg)
#define logmesg(msg) logger_mesg(&LOGGER, msg)
#define loginfo(msg) logger_info(&LOGGER, msg)
#define loglog(msg) logger_log(&LOGGER, msg)
#define logdebug(msg) logger_debug(&LOGGER, msg)
#define logtrace(msg) logger_trace(&LOGGER, msg)

// Format versions (with format specifiers)
#define logerror_fmt(...) logger_error_fmt(&LOGGER, __VA_ARGS__)
#define logwarn_fmt(...) logger_warn_fmt(&LOGGER, __VA_ARGS__)
#define logmesg_fmt(...) logger_mesg_fmt(&LOGGER, __VA_ARGS__)
#define loginfo_fmt(...) logger_info_fmt(&LOGGER, __VA_ARGS__)
#define loglog_fmt(...) logger_log_fmt(&LOGGER, __VA_ARGS__)
#define logdebug_fmt(...) logger_debug_fmt(&LOGGER, __VA_ARGS__)
#define logtrace_fmt(...) logger_trace_fmt(&LOGGER, __VA_ARGS__)

#ifdef __cplusplus
}
#endif