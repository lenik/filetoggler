#include "logger.h"
#include <stdio.h>
#include <stdarg.h>

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_ERROR   "\033[1;31m"  // Bright red
#define COLOR_WARN    "\033[1;33m"  // Bright yellow
#define COLOR_MESG    "\033[0m"     // Default/white
#define COLOR_INFO    "\033[1;32m"  // Bright green
#define COLOR_LOG     "\033[1;37m"  // Gray
#define COLOR_DEBUG   "\033[1;90m"  // Light gray
#define COLOR_TRACE   "\033[1;90m"  // Light gray

void logger_set_level(logger_t* logger, int value) {
    if (logger) {
        logger->level = value;
    }
}

int logger_get_level(const logger_t* logger) {
    return logger ? logger->level : 1;
}

void logger_more(logger_t* logger) {
    if (logger) {
        logger->level++;
    }
}

void logger_less(logger_t* logger) {
    if (logger) {
        logger->level--;
    }
}

void logger_reset(logger_t* logger) {
    if (logger) {
        logger->level = logger->level_default;
    }
}

// Logging functions (without format specifiers)
void logger_error(const logger_t* logger, const char* message) {
    if (!logger) return;
    fprintf(stderr, "%s%s ERROR: %s%s\n", 
            logger->color_error, logger->header, message, logger->color_reset);
}

void logger_warn(const logger_t* logger, const char* message) {
    if (!logger) return;
    if (logger->level >= 0) {
        fprintf(stderr, "%s%s WARN: %s%s\n", 
                logger->color_warn, logger->header, message, logger->color_reset);
    }
}

void logger_mesg(const logger_t* logger, const char* message) {
    if (!logger) return;
    if (logger->level >= 1) {
        printf("%s%s %s%s\n", 
               logger->color_mesg, logger->header, message, logger->color_reset);
    }
}

void logger_info(const logger_t* logger, const char* message) {
    if (!logger) return;
    if (logger->level >= 2) {
        printf("%s%s INFO: %s%s\n", 
               logger->color_info, logger->header, message, logger->color_reset);
    }
}

void logger_log(const logger_t* logger, const char* message) {
    if (!logger) return;
    if (logger->level >= 3) {
        printf("%s%s LOG: %s%s\n", 
               logger->color_log, logger->header, message, logger->color_reset);
    }
}

void logger_debug(const logger_t* logger, const char* message) {
    if (!logger) return;
    if (logger->level >= 4) {
        printf("%s%s DEBUG: %s%s\n", 
               logger->color_debug, logger->header, message, logger->color_reset);
    }
}

// Format versions (with format specifiers)
void logger_error_fmt(const logger_t* logger, const char* format, ...) {
    if (!logger) return;
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s%s ERROR: ", logger->color_error, logger->header);
    vfprintf(stderr, format, args);
    fprintf(stderr, "%s\n", logger->color_reset);
    va_end(args);
}

void logger_warn_fmt(const logger_t* logger, const char* format, ...) {
    if (!logger) return;
    if (logger->level >= 0) {
        va_list args;
        va_start(args, format);
        fprintf(stderr, "%s%s WARN: ", logger->color_warn, logger->header);
        vfprintf(stderr, format, args);
        fprintf(stderr, "%s\n", logger->color_reset);
        va_end(args);
    }
}

void logger_mesg_fmt(const logger_t* logger, const char* format, ...) {
    if (!logger) return;
    if (logger->level >= 1) {
        va_list args;
        va_start(args, format);
        printf("%s%s ", logger->color_mesg, logger->header);
        vprintf(format, args);
        printf("%s\n", logger->color_reset);
        va_end(args);
    }
}

void logger_info_fmt(const logger_t* logger, const char* format, ...) {
    if (!logger) return;
    if (logger->level >= 2) {
        va_list args;
        va_start(args, format);
        printf("%s%s INFO: ", logger->color_info, logger->header);
        vprintf(format, args);
        printf("%s\n", logger->color_reset);
        va_end(args);
    }
}

void logger_log_fmt(const logger_t* logger, const char* format, ...) {
    if (!logger) return;
    if (logger->level >= 3) {
        va_list args;
        va_start(args, format);
        printf("%s%s LOG: ", logger->color_log, logger->header);
        vprintf(format, args);
        printf("%s\n", logger->color_reset);
        va_end(args);
    }
}

void logger_debug_fmt(const logger_t* logger, const char* format, ...) {
    if (!logger) return;
    if (logger->level >= 4) {
        va_list args;
        va_start(args, format);
        printf("%s%s DEBUG: ", logger->color_debug, logger->header);
        vprintf(format, args);
        printf("%s\n", logger->color_reset);
        va_end(args);
    }
}

void logger_trace_fmt(const logger_t* logger, const char* format, ...) {
    if (!logger) return;
    if (logger->level >= 5) {
        va_list args;
        va_start(args, format);
        printf("%s%s TRACE: ", logger->color_trace, logger->header);
        vprintf(format, args);
        printf("%s\n", logger->color_reset);
        va_end(args);
    }
}