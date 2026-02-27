#include "args.h"
#include "string_list.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <fnmatch.h>
#include <stdbool.h>

#define ARGS_INITIAL_CAPACITY 16
#define MAX_PATH 1024

// Internal helper: skip whitespace
static const char* skip_whitespace(const char* p) {
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

// Check if a string contains wildcard characters (*, ?, [a-z], [[:class:]], or {})
static int contains_wildcard(const char* str) {
    if (!str) return 0;
    // Check for basic wildcards
    if (strchr(str, '*') != NULL || strchr(str, '?') != NULL) {
        return 1;
    }
    // Check for character sets [a-z] or character classes [[:class:]]
    if (strchr(str, '[') != NULL) {
        return 1;
    }
    return 0;
}

// Check if a string contains brace expansion patterns {foo,bar}
static int contains_brace(const char* str) {
    if (!str) return 0;
    return (strchr(str, '{') != NULL);
}

// Expand brace pattern {foo,bar,baz} into array of strings
static int expand_brace(const char* pattern, char*** expansions, size_t* exp_count, size_t* exp_capacity) {
    if (!pattern || !expansions || !exp_count || !exp_capacity) {
        return -1;
    }
    
    const char* brace_start = strchr(pattern, '{');
    if (!brace_start) {
        return 0;  // No brace to expand
    }
    
    const char* brace_end = strchr(brace_start, '}');
    if (!brace_end) {
        return 0;  // Unmatched brace, don't expand
    }
    
    // Extract prefix (before {)
    size_t prefix_len = brace_start - pattern;
    char* prefix = NULL;
    if (prefix_len > 0) {
        prefix = malloc(prefix_len + 1);
        if (!prefix) return -1;
        strncpy(prefix, pattern, prefix_len);
        prefix[prefix_len] = '\0';
    }
    
    // Extract suffix (after })
    const char* suffix = brace_end + 1;
    size_t suffix_len = strlen(suffix);
    
    // Extract brace content
    size_t brace_content_len = brace_end - brace_start - 1;
    char* brace_content = malloc(brace_content_len + 1);
    if (!brace_content) {
        if (prefix) free(prefix);
        return -1;
    }
    strncpy(brace_content, brace_start + 1, brace_content_len);
    brace_content[brace_content_len] = '\0';
    
    // Parse comma-separated items in brace content
    const char* p = brace_content;
    while (*p) {
        // Skip whitespace
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        
        // Find end of current item (comma or end)
        const char* item_start = p;
        const char* item_end = p;
        int in_quotes = 0;
        char quote_char = 0;
        
        while (*item_end) {
            if (!in_quotes && (*item_end == '\'' || *item_end == '"')) {
                in_quotes = 1;
                quote_char = *item_end;
                item_end++;
            } else if (in_quotes && *item_end == quote_char) {
                in_quotes = 0;
                quote_char = 0;
                item_end++;
            } else if (!in_quotes && *item_end == ',') {
                break;
            } else {
                item_end++;
            }
        }
        
        size_t item_len = item_end - item_start;
        
        // Build expanded string: prefix + item + suffix
        size_t total_len = (prefix ? strlen(prefix) : 0) + item_len + suffix_len + 1;
        char* expanded = malloc(total_len);
        if (!expanded) {
            free(brace_content);
            if (prefix) free(prefix);
            // Free already allocated expansions
            for (size_t i = 0; i < *exp_count; i++) {
                free((*expansions)[i]);
            }
            if (*expansions) free(*expansions);
            return -1;
        }
        
        expanded[0] = '\0';
        if (prefix) strcat(expanded, prefix);
        strncat(expanded, item_start, item_len);
        strcat(expanded, suffix);
        
        // Add to expansions array
        if (*exp_count >= *exp_capacity) {
            size_t new_capacity = (*exp_capacity == 0) ? 8 : *exp_capacity * 2;
            char** new_expansions = realloc(*expansions, new_capacity * sizeof(char*));
            if (!new_expansions) {
                free(expanded);
                free(brace_content);
                if (prefix) free(prefix);
                for (size_t i = 0; i < *exp_count; i++) {
                    free((*expansions)[i]);
                }
                if (*expansions) free(*expansions);
                return -1;
            }
            *expansions = new_expansions;
            *exp_capacity = new_capacity;
        }
        
        (*expansions)[*exp_count] = expanded;
        (*exp_count)++;
        
        // Move to next item
        if (*item_end == ',') {
            p = item_end + 1;
        } else {
            break;
        }
    }
    
    free(brace_content);
    if (prefix) free(prefix);
    
    return (*exp_count > 0) ? 1 : 0;
}

// Callback context for wildcard expansion
typedef struct {
    string_list* match_list;
    const char* file_pattern;
    const char* pattern_dir;  // Directory part of original pattern (NULL if no directory)
    const char* current_dir;  // Current working directory for relative paths
} wildcard_context;

// Callback function for wildcard expansion
static bool wildcard_callback(const char *item, void *item_context) {
    wildcard_context* ctx = (wildcard_context*)item_context;
    
    // Extract filename from full path
    const char* filename = strrchr(item, '/');
    if (!filename) filename = item;
    else filename++;  // Skip the '/'
    
    // Use fnmatch to match pattern
    if (fnmatch(ctx->file_pattern, filename, 0) == 0) {
        // Build the result path based on whether pattern had a directory component
        char result_path[1024];
        if (ctx->pattern_dir && strlen(ctx->pattern_dir) > 0) {
            // Pattern had directory (e.g., "image/*"), return relative path from current dir
            // result = pattern_dir/filename (e.g., "image/Indicators.png")
            snprintf(result_path, sizeof(result_path), "%s/%s", ctx->pattern_dir, filename);
        } else {
            // Pattern had no directory (e.g., "*"), return just filename
            // result = filename (e.g., "Indicators.png")
            snprintf(result_path, sizeof(result_path), "%s", filename);
        }
        
        // Add to match list (string_list_append duplicates the string)
        // string_list_append returns 1 on success, 0 on failure
        if (string_list_append(ctx->match_list, result_path) == 0) {
            return false;  // Cancel enumeration on memory error
        }
    }
    return true;  // Continue enumeration
}

// Expand wildcard pattern to matching files using enumeration function
static int expand_wildcard(const char* pattern, enum_fn* enum_func, void *enum_context, void *item_context, char*** matches, size_t* match_count, size_t* match_capacity) {
    if (!pattern || !matches || !match_count || !match_capacity) {
        return -1;
    }
    
    // If no enum function provided, don't expand wildcards
    if (!enum_func) {
        return 0;
    }
    
    // Extract filename pattern and prefix, and directory part
    char file_pattern[256];
    char filename_prefix[256] = "";
    char pattern_dir[256] = "";
    
    // Find last '/' to separate directory and pattern
    const char* last_slash = strrchr(pattern, '/');
    if (last_slash) {
        // Pattern has directory component (e.g., "image/*")
        size_t dir_len = last_slash - pattern;
        if (dir_len > 0 && dir_len < sizeof(pattern_dir)) {
            strncpy(pattern_dir, pattern, dir_len);
            pattern_dir[dir_len] = '\0';
        }
        strncpy(file_pattern, last_slash + 1, sizeof(file_pattern) - 1);
        file_pattern[sizeof(file_pattern) - 1] = '\0';
    } else {
        // No directory in pattern (e.g., "*"), use pattern as file pattern
        pattern_dir[0] = '\0';  // Empty directory
        strncpy(file_pattern, pattern, sizeof(file_pattern) - 1);
        file_pattern[sizeof(file_pattern) - 1] = '\0';
    }
    
    // Extract filename prefix (characters before first wildcard) for optimization
    // This is a hint to enum_fn to filter early
    const char* first_wildcard = strpbrk(file_pattern, "*?[");
    if (first_wildcard && first_wildcard > file_pattern) {
        size_t prefix_len = first_wildcard - file_pattern;
        if (prefix_len < sizeof(filename_prefix)) {
            strncpy(filename_prefix, file_pattern, prefix_len);
            filename_prefix[prefix_len] = '\0';
        }
    } else if (strlen(file_pattern) > 0 && strpbrk(file_pattern, "*?[") == NULL) {
        // No wildcards, entire pattern is the prefix
        strncpy(filename_prefix, file_pattern, sizeof(filename_prefix) - 1);
        filename_prefix[sizeof(filename_prefix) - 1] = '\0';
    }
    
    // Create string_list for matches
    string_list* match_list = string_list_create(8);
    if (!match_list) {
        return -1;
    }
    
    // Set up context for callback
    // Always use our local context to ensure match_list is properly initialized
    wildcard_context ctx;
    ctx.match_list = match_list;
    ctx.file_pattern = file_pattern;
    ctx.pattern_dir = (strlen(pattern_dir) > 0) ? pattern_dir : NULL;
    // Get current directory from enum_context (it's the current working directory)
    ctx.current_dir = enum_context ? (const char*)enum_context : NULL;
    
    // Always use our local context (item_context is not used for wildcard expansion)
    void* callback_context = &ctx;
    
    // Determine which directory to enumerate
    // If pattern had a directory, use that; otherwise use enum_context (current dir)
    void* enum_dir_context = enum_context;
    if (strlen(pattern_dir) > 0) {
        // Pattern had directory, need to resolve it relative to current dir
        // Build full path: current_dir/pattern_dir
        static char resolved_dir[1024];
        if (enum_context && strlen((const char*)enum_context) > 0) {
            const char* current = (const char*)enum_context;
            if (strcmp(current, "/") == 0) {
                // At root, pattern_dir is already relative to root
                snprintf(resolved_dir, sizeof(resolved_dir), "/%s", pattern_dir);
            } else {
                // Not at root, build current_dir/pattern_dir
                snprintf(resolved_dir, sizeof(resolved_dir), "%s/%s", current, pattern_dir);
            }
        } else {
            // No current dir, use pattern_dir as absolute or relative
            if (pattern_dir[0] == '/') {
                strncpy(resolved_dir, pattern_dir, sizeof(resolved_dir) - 1);
                resolved_dir[sizeof(resolved_dir) - 1] = '\0';
            } else {
                snprintf(resolved_dir, sizeof(resolved_dir), "/%s", pattern_dir);
            }
        }
        enum_dir_context = resolved_dir;
    }
    
    // Call enumeration function
    const char* prefix_arg = (strlen(filename_prefix) > 0) ? filename_prefix : NULL;
    int result = enum_func(enum_dir_context, prefix_arg, file_pattern, wildcard_callback, callback_context);
    
    // Convert string_list to array format expected by caller
    if (result == 0) {
        size_t list_size = string_list_size(match_list);
        if (list_size > 0) {
            // Allocate array for matches
            char** match_array = malloc(list_size * sizeof(char*));
            if (!match_array) {
                string_list_free(match_list);
                return -1;
            }
            
            // Copy strings from list to array
            // Note: string_list owns the strings, so we need to duplicate them
            // before freeing the list
            size_t valid_count = 0;
            for (size_t i = 0; i < list_size; i++) {
                const char* str = string_list_get(match_list, i);
                if (!str) {
                    // Skip NULL entries (shouldn't happen, but be defensive)
                    continue;
                }
                match_array[valid_count] = strdup(str);  // Duplicate for caller ownership
                if (!match_array[valid_count]) {
                    // Free what we've allocated so far
                    for (size_t j = 0; j < valid_count; j++) {
                        free(match_array[j]);
                    }
                    free(match_array);
                    string_list_free(match_list);
                    return -1;
                }
                valid_count++;
            }
            
            // Update match count to reflect actual valid entries
            if (valid_count < list_size) {
                // Some entries were NULL, adjust the array
                *match_count = valid_count;
                *match_capacity = valid_count;
            } else {
                *match_count = list_size;
                *match_capacity = list_size;
            }
            
            *matches = match_array;
        } else {
            *matches = NULL;
            *match_count = 0;
            *match_capacity = 0;
        }
    }
    
    // Free the string_list (this will free the strings it owns)
    // But we've already duplicated them to the array, so that's OK
    string_list_free(match_list);
    
    return (result == 0) ? 0 : -1;
}

// Internal helper: parse a single argument
static char* parse_arg(const char** line_ptr, int* error) {
    const char* line = *line_ptr;
    char quote_char = 0;
    int escaped = 0;
    size_t len = 0;
    size_t capacity = 64;
    char* result = malloc(capacity);
    if (!result) {
        *error = -1;
        return NULL;
    }
    
    // Skip leading whitespace
    line = skip_whitespace(line);
    if (!*line) {
        free(result);
        *line_ptr = line;
        return NULL;
    }
    
    while (*line) {
        if (escaped) {
            // Handle escaped character
            if (len + 1 >= capacity) {
                capacity *= 2;
                char* new_result = realloc(result, capacity);
                if (!new_result) {
                    free(result);
                    *error = -1;
                    return NULL;
                }
                result = new_result;
            }
            result[len++] = *line;
            escaped = 0;
            line++;
        } else if (*line == '\\') {
            // Escape character
            escaped = 1;
            line++;
        } else if (quote_char) {
            // Inside quotes - wildcards are literal inside quotes
            if (*line == quote_char) {
                // End of quoted string
                quote_char = 0;
                line++;
            } else {
                // Regular character inside quotes
                if (len + 1 >= capacity) {
                    capacity *= 2;
                    char* new_result = realloc(result, capacity);
                    if (!new_result) {
                        free(result);
                        *error = -1;
                        return NULL;
                    }
                    result = new_result;
                }
                result[len++] = *line;
                line++;
            }
        } else if (*line == '\'' || *line == '"') {
            // Start of quoted string (quote is not included in result)
            quote_char = *line;
            line++;
        } else if (isspace((unsigned char)*line)) {
            // End of argument
            break;
        } else {
            // Regular character
            if (len + 1 >= capacity) {
                capacity *= 2;
                char* new_result = realloc(result, capacity);
                if (!new_result) {
                    free(result);
                    *error = -1;
                    return NULL;
                }
                result = new_result;
            }
            result[len++] = *line;
            line++;
        }
    }
    
    // Check for unmatched quote
    if (quote_char) {
        free(result);
        *error = -1;
        return NULL;
    }
    
    // Null-terminate
    result[len] = '\0';
    
    // Update line pointer
    *line_ptr = line;
    
    return result;
}

int args_parse(enum_fn* enum_func, void *enum_context, void *item_context, const char* line, int* argc, char*** argv) {
    if (!line || !argc || !argv) {
        return -1;
    }
    
    *argc = 0;
    *argv = NULL;
    
    char** args = NULL;
    size_t capacity = ARGS_INITIAL_CAPACITY;
    size_t count = 0;
    int error = 0;
    const char* p = line;
    
    args = malloc(capacity * sizeof(char*));
    if (!args) {
        return -1;
    }
    
    // First pass: parse arguments (may contain wildcards)
    char** raw_args = NULL;
    size_t raw_count = 0;
    size_t raw_capacity = ARGS_INITIAL_CAPACITY;
    
    raw_args = malloc(raw_capacity * sizeof(char*));
    if (!raw_args) {
        free(args);
        return -1;
    }
    
    // Parse all arguments first
    while (*p) {
        p = skip_whitespace(p);
        if (!*p) {
            break;
        }
        
        char* arg = parse_arg(&p, &error);
        if (error) {
            // Free already parsed arguments
            for (size_t i = 0; i < raw_count; i++) {
                free(raw_args[i]);
            }
            free(raw_args);
            free(args);
            return -1;
        }
        
        if (!arg) {
            break;
        }
        
        // Add to raw args array
        if (raw_count >= raw_capacity) {
            raw_capacity *= 2;
            char** new_raw_args = realloc(raw_args, raw_capacity * sizeof(char*));
            if (!new_raw_args) {
                free(arg);
                for (size_t i = 0; i < raw_count; i++) {
                    free(raw_args[i]);
                }
                free(raw_args);
                free(args);
                return -1;
            }
            raw_args = new_raw_args;
        }
        
        raw_args[raw_count++] = arg;
    }
    
    // Second pass: expand braces first, then wildcards
    for (size_t i = 0; i < raw_count; i++) {
        char* arg = raw_args[i];
        
        // First, expand braces {foo,bar}
        if (contains_brace(arg)) {
            char** brace_expansions = NULL;
            size_t brace_count = 0;
            size_t brace_capacity = 0;
            
            if (expand_brace(arg, &brace_expansions, &brace_count, &brace_capacity) > 0) {
                // Process each brace expansion - they may contain wildcards
                for (size_t j = 0; j < brace_count; j++) {
                    char* expanded_arg = brace_expansions[j];
                    
                    // Check if this expansion contains wildcards
                    if (contains_wildcard(expanded_arg)) {
                        // Expand wildcard in this brace expansion
                        char** matches = NULL;
                        size_t match_count = 0;
                        size_t match_capacity = 0;
                        
                        if (expand_wildcard(expanded_arg, enum_func, enum_context, item_context, &matches, &match_count, &match_capacity) == 0) {
                            if (match_count > 0) {
                                // Add all matches to args
                                for (size_t k = 0; k < match_count; k++) {
                                    if (count >= capacity) {
                                        capacity *= 2;
                                        char** new_args = realloc(args, capacity * sizeof(char*));
                                        if (!new_args) {
                                            // Free matches
                                            for (size_t m = 0; m < match_count; m++) {
                                                free(matches[m]);
                                            }
                                            free(matches);
                                            // Free brace expansions
                                            for (size_t m = 0; m < brace_count; m++) {
                                                free(brace_expansions[m]);
                                            }
                                            free(brace_expansions);
                                            // Free already processed args
                                            for (size_t m = 0; m < count; m++) {
                                                free(args[m]);
                                            }
                                            free(args);
                                            // Free remaining raw args
                                            for (size_t m = i + 1; m < raw_count; m++) {
                                                free(raw_args[m]);
                                            }
                                            free(raw_args);
                                            return -1;
                                        }
                                        args = new_args;
                                    }
                                    args[count++] = matches[k];
                                }
                                free(matches);  // Free array, strings are in args now
                                free(expanded_arg);  // Free expanded_arg since it's replaced by matches
                            } else {
                                // No matches, keep original pattern
                                if (count >= capacity) {
                                    capacity *= 2;
                                    char** new_args = realloc(args, capacity * sizeof(char*));
                                    if (!new_args) {
                                        // Free brace expansions
                                        for (size_t m = 0; m < brace_count; m++) {
                                            free(brace_expansions[m]);
                                        }
                                        free(brace_expansions);
                                        // Free already processed args
                                        for (size_t m = 0; m < count; m++) {
                                            free(args[m]);
                                        }
                                        free(args);
                                        // Free remaining raw args
                                        for (size_t m = i + 1; m < raw_count; m++) {
                                            free(raw_args[m]);
                                        }
                                        free(raw_args);
                                        return -1;
                                    }
                                    args = new_args;
                                }
                                args[count++] = expanded_arg;  // Keep original pattern
                            }
                        } else {
                            // Wildcard expansion failed, keep original
                            if (count >= capacity) {
                                capacity *= 2;
                                char** new_args = realloc(args, capacity * sizeof(char*));
                                if (!new_args) {
                                    // Free brace expansions
                                    for (size_t m = 0; m < brace_count; m++) {
                                        free(brace_expansions[m]);
                                    }
                                    free(brace_expansions);
                                    // Free already processed args
                                    for (size_t m = 0; m < count; m++) {
                                        free(args[m]);
                                    }
                                    free(args);
                                    // Free remaining raw args
                                    for (size_t m = i + 1; m < raw_count; m++) {
                                        free(raw_args[m]);
                                    }
                                    free(raw_args);
                                    return -1;
                                }
                                args = new_args;
                            }
                            args[count++] = expanded_arg;
                        }
                    } else {
                        // No wildcards, add as-is
                        if (count >= capacity) {
                            capacity *= 2;
                            char** new_args = realloc(args, capacity * sizeof(char*));
                            if (!new_args) {
                                // Free brace expansions
                                for (size_t k = 0; k < brace_count; k++) {
                                    free(brace_expansions[k]);
                                }
                                free(brace_expansions);
                                // Free already processed args
                                for (size_t k = 0; k < count; k++) {
                                    free(args[k]);
                                }
                                free(args);
                                // Free remaining raw args
                                for (size_t k = i + 1; k < raw_count; k++) {
                                    free(raw_args[k]);
                                }
                                free(raw_args);
                                return -1;
                            }
                            args = new_args;
                        }
                        args[count++] = expanded_arg;
                    }
                }
                free(brace_expansions);  // Free array, strings are in args now
                free(arg);  // Free original arg, replaced by expansions
                continue;  // Move to next raw arg
            } else {
                // Brace expansion failed or no braces, fall through to wildcard expansion
                if (brace_expansions) {
                    for (size_t k = 0; k < brace_count; k++) {
                        free(brace_expansions[k]);
                    }
                    free(brace_expansions);
                }
            }
        }
        
        // Then, expand wildcards (including character sets and classes)
        if (contains_wildcard(arg)) {
            // Expand wildcard
            char** matches = NULL;
            size_t match_count = 0;
            size_t match_capacity = 0;
            
            if (expand_wildcard(arg, enum_func, enum_context, item_context, &matches, &match_count, &match_capacity) == 0) {
                if (match_count > 0) {
                    // Add all matches to args
                    for (size_t j = 0; j < match_count; j++) {
                        if (count >= capacity) {
                            capacity *= 2;
                            char** new_args = realloc(args, capacity * sizeof(char*));
                            if (!new_args) {
                                // Free matches
                                for (size_t k = 0; k < match_count; k++) {
                                    free(matches[k]);
                                }
                                free(matches);
                                // Free already processed args
                                for (size_t k = 0; k < count; k++) {
                                    free(args[k]);
                                }
                                free(args);
                                // Free remaining raw args
                                for (size_t k = i + 1; k < raw_count; k++) {
                                    free(raw_args[k]);
                                }
                                free(raw_args);
                                return -1;
                            }
                            args = new_args;
                        }
                        args[count++] = matches[j];
                    }
                    free(matches);  // Free array, but not strings (they're in args now)
                } else {
                    // No matches, keep original pattern (let command handle "no match" error)
                    if (count >= capacity) {
                        capacity *= 2;
                        char** new_args = realloc(args, capacity * sizeof(char*));
                        if (!new_args) {
                            for (size_t k = 0; k < count; k++) {
                                free(args[k]);
                            }
                            free(args);
                            for (size_t k = i + 1; k < raw_count; k++) {
                                free(raw_args[k]);
                            }
                            free(raw_args);
                            return -1;
                        }
                        args = new_args;
                    }
                    args[count++] = arg;  // Keep original pattern
                }
            } else {
                // Expansion failed, keep original
                if (count >= capacity) {
                    capacity *= 2;
                    char** new_args = realloc(args, capacity * sizeof(char*));
                    if (!new_args) {
                        for (size_t k = 0; k < count; k++) {
                            free(args[k]);
                        }
                        free(args);
                        for (size_t k = i + 1; k < raw_count; k++) {
                            free(raw_args[k]);
                        }
                        free(raw_args);
                        return -1;
                    }
                    args = new_args;
                }
                args[count++] = arg;  // Keep original pattern
            }
        } else {
            // No wildcard, add as-is
            if (count >= capacity) {
                capacity *= 2;
                char** new_args = realloc(args, capacity * sizeof(char*));
                if (!new_args) {
                    for (size_t k = 0; k < count; k++) {
                        free(args[k]);
                    }
                    free(args);
                    for (size_t k = i + 1; k < raw_count; k++) {
                        free(raw_args[k]);
                    }
                    free(raw_args);
                    return -1;
                }
                args = new_args;
            }
            args[count++] = arg;
        }
    }
    
    // Free raw_args array (strings are now in args or freed)
    free(raw_args);
    
    // Add null terminator after last argument (required for functions like getopt_long)
    // Ensure we have space for the null terminator
    if (count >= capacity) {
        capacity += 1;
        char** new_args = realloc(args, capacity * sizeof(char*));
        if (!new_args) {
            for (size_t i = 0; i < count; i++) {
                free(args[i]);
            }
            free(args);
            return -1;
        }
        args = new_args;
    }
    args[count] = NULL;  // Null terminator
    
    *argc = (int)count;
    *argv = args;
    
    return 0;
}

void args_free(char** argv, int argc) {
    if (!argv) {
        return;
    }
    
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}

string_list* args_parse_to_list(enum_fn* enum_func, void *enum_context, void *item_context, const char* line) {
    int argc = 0;
    char** argv = NULL;
    
    if (args_parse(enum_func, enum_context, item_context, line, &argc, &argv) != 0) {
        return NULL;
    }
    
    string_list* list = string_list_create(argc);
    if (!list) {
        args_free(argv, argc);
        return NULL;
    }
    
    for (int i = 0; i < argc; i++) {
        if (!string_list_add(list, argv[i])) {
            string_list_free(list);
            args_free(argv, argc);
            return NULL;
        }
    }
    
    // Free argv (but not the strings, as they're now owned by string_list)
    // Actually, we need to free the strings since string_list_add duplicates them
    args_free(argv, argc);
    
    return list;
}
