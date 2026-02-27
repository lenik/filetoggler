#include "format_backtrace.h"

#include "format_gdb.h"
#include "stackdump.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Backtrace format: "=== Thread (current) ===" then "#0  0x... in ... at file:line" lines. */
static bool is_backtrace_thread_header(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    return strstr(p, "===") != NULL && strstr(p, "Thread") != NULL;
}

/* Extract name from "=== Thread (current) ===" -> "current", or use whole line as name. */
static char *backtrace_thread_name_from_line(const char *line) {
    const char *p = strstr(line, "(");
    if (p) {
        const char *end = strchr(p + 1, ')');
        if (end && end > p + 1) {
            size_t len = (size_t)(end - (p + 1));
            char *name = (char *)malloc(len + 1);
            if (name) {
                memcpy(name, p + 1, len);
                name[len] = '\0';
                return name;
            }
        }
    }
    char *name = (char *)malloc(8);
    if (name) strcpy(name, "current");
    return name;
}

static stack_frame_t *backtrace_stack_frame_parse(const char *line) {
    return gdb_output_stack_frame_parse(line);
}

GList *backtrace_parse(FILE *in) {
    GList *result = NULL;
    char *line = NULL;
    size_t cap = 0;
    thread_info_t *current = NULL;

    while (getline(&line, &cap, in) > 0) {
        if (is_backtrace_thread_header(line)) {
            current = (thread_info_t *)calloc(1, sizeof(thread_info_t));
            if (!current) continue;
            current->thread_id = 0;
            current->lwp = 0;
            current->name = backtrace_thread_name_from_line(line);
            if (!current->name) current->name = strdup("?");
            current->frames = NULL;
            result = g_list_append(result, current);
            continue;
        }

        if (current && line[0] == '#') {
            stack_frame_t *f = backtrace_stack_frame_parse(line);
            if (f) {
                current->frames = g_list_append(current->frames, f);
            }
        }
    }
    free(line);
    return result;
}

void backtrace_highlight_fd(FILE *in, FILE *out, const stackdump_color_schema_t *color_schema) {
    const stackdump_color_schema_t *s = color_schema ? color_schema : &stackdump_color_schema_default;

    GList *threads = backtrace_parse(in);
    thread_info_list_format(out, threads, s);
    thread_info_list_free(threads);
}

void backtrace_highlight_file(const char *input_file, const char *output_file, const stackdump_color_schema_t *color_schema) {
    FILE *fp = fopen(input_file, "r");
    if (!fp) return;
    FILE *out = fopen(output_file, "w");
    if (!out) {
        fclose(fp);
        return;
    }
    backtrace_highlight_fd(fp, out, color_schema);
    fclose(fp);
    fclose(out);
}
