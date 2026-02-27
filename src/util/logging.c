#include "logging-sf.h"
#include "logger.h"
#include <stdarg.h>

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_ERROR   "\033[1;31m"  // Bright red
#define COLOR_WARN    "\033[1;33m"  // Bright yellow
#define COLOR_MESG    "\033[0m"     // Default/white
#define COLOR_INFO    "\033[1;32m"  // Bright green
#define COLOR_LOG     "\033[1;37m"  // Gray
#define COLOR_DEBUG   "\033[1;90m"  // Light gray

// Default logger instance
logger_t ft_logger = {
    .level = 1,
    .level_default = 1,
    .header = "[ft]",
    .color_error = COLOR_ERROR,
    .color_warn = COLOR_WARN,
    .color_mesg = COLOR_MESG,
    .color_info = COLOR_INFO,
    .color_log = COLOR_LOG,
    .color_debug = COLOR_DEBUG,
    .color_reset = COLOR_RESET
};
