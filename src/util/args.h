#ifndef ARGS_H
#define ARGS_H

#include <stddef.h>
#include <stdbool.h>
#include "string_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callback function type for directory enumeration.
 * Called for each item in the directory.
 * 
 * @param item The full path of the item (directory path + item name)
 * @param item_context Context data passed from args_parse
 * @return true to continue enumeration, false to cancel
 */
typedef bool enum_callback(const char *item, void *item_context);

/**
 * Directory enumeration function type.
 * Enumerates items in a directory matching the prefix.
 * 
 * @param enum_context Context data for enumeration (e.g., directory path)
 * @param prefix Filename prefix that must be matched (NULL matches all)
 * @param pattern Optimization hint pattern (optional, may be NULL, not required to match)
 * @param item_cb Callback function called for each matching item
 * @param item_context Context data to pass to callback
 * @return 0 on success, -1 on error
 */
typedef int enum_fn(void *enum_context, const char *prefix, const char *pattern, enum_callback item_cb, void *item_context);

/**
 * Parse a command line string into an array of arguments.
 * Handles single quotes ('), double quotes ("), escapes, and wildcard expansion.
 * Quotes are removed from the parsed arguments.
 * Wildcards (* and ?) are expanded to matching files using the enumeration function.
 * 
 * @param enum_func Directory enumeration function for wildcard expansion (may be NULL to disable wildcard expansion)
 * @param enum_context Context data for enumeration function (e.g., directory path)
 * @param item_context Context data to pass to enumeration callback
 * @param line The command line string to parse
 * @param argc Output parameter for the number of arguments
 * @param argv Output parameter for the array of argument strings
 * @return 0 on success, -1 on error (e.g., unmatched quotes)
 * 
 * The caller is responsible for freeing the returned argv array and strings
 * using args_free().
 */
int args_parse(enum_fn* enum_func, void *enum_context, void *item_context, const char* line, int* argc, char*** argv);

/**
 * Free the arguments array returned by args_parse().
 * 
 * @param argv The arguments array
 * @param argc The number of arguments
 */
void args_free(char** argv, int argc);

/**
 * Parse a command line string into a string_list.
 * Handles single quotes ('), double quotes ("), escapes, and wildcard expansion.
 * Quotes are removed from the parsed arguments.
 * 
 * @param enum_func Directory enumeration function for wildcard expansion (may be NULL to disable wildcard expansion)
 * @param enum_context Context data for enumeration function (e.g., directory path)
 * @param item_context Context data to pass to enumeration callback
 * @param line The command line string to parse
 * @return A string_list* containing the parsed arguments, or NULL on error
 * 
 * The caller is responsible for freeing the returned string_list using
 * string_list_free().
 */
string_list* args_parse_to_list(enum_fn* enum_func, void *enum_context, void *item_context, const char* line);

#ifdef __cplusplus
}
#endif

#endif /* ARGS_H */

