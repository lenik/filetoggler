#include "format_gdb.h"

#include "stackdump.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Parse "Thread 2 (Thread 0x7f1a8b0196c0 (LWP 3116497) \"pool-1\")" into thread_info_t. */
thread_info_t *gdb_output_thread_info_parse(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "Thread ", 7) != 0) return NULL;

    thread_info_t *t = (thread_info_t *)calloc(1, sizeof(thread_info_t));
    if (!t) return NULL;
    t->frames = NULL;

    p += 7;
    if (!isdigit((unsigned char)*p)) { thread_info_free(t); return NULL; }
    t->thread_id = (int)strtol(p, (char **)&p, 10);

    /* (LWP N) can appear anywhere in the rest of the line */
    const char *lwp = strstr(p, "(LWP ");
    if (lwp) {
        lwp += 5;
        if (isdigit((unsigned char)*lwp))
            t->lwp = (int)strtol(lwp, NULL, 10);
    }

    /* Name in quotes */
    const char *q = strchr(line, '"');
    if (q && q[1]) {
        const char *q2 = strchr(q + 1, '"');
        if (q2 && q2 > q + 1) {
            size_t len = (size_t)(q2 - (q + 1));
            t->name = (char *)malloc(len + 1);
            if (t->name) {
                memcpy(t->name, q + 1, len);
                t->name[len] = '\0';
            }
        }
    }
    if (!t->name) {
        t->name = (char *)malloc(1);
        if (t->name) t->name[0] = '\0';
    }
    return t;
}

/* Parse "#3 0x..." or "#30x..." (no space between #N and 0x) into stack_frame_t. */
stack_frame_t *gdb_output_stack_frame_parse(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '#' || !isdigit((unsigned char)p[1])) return NULL;

    stack_frame_t *f = (stack_frame_t *)calloc(1, sizeof(stack_frame_t));
    if (!f) return NULL;
    f->raw_line = strdup(line);
    if (!f->raw_line) { stack_frame_free(f); return NULL; }

    /* #N - digits; if followed by 'x'/'X' and last digit is '0', then N is frame and rest is 0xADDR */
    p++;
    int num = 0;
    const char *dig_end = p;
    while (isdigit((unsigned char)*dig_end)) num = num * 10 + (*dig_end++ - '0');
    if (num > 0 && (*dig_end == 'x' || *dig_end == 'X') && (num % 10 == 0)) {
        f->frame_number = num / 10;
        p = dig_end - 1; /* point to '0' before 0x */
    } else {
        f->frame_number = num;
        p = dig_end;
    }

    /* 0x... */
    while (*p == ' ' || *p == '\t') p++;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        const char *addr_start = p;
        p += 2;
        while (isxdigit((unsigned char)*p)) p++;
        size_t len = (size_t)(p - addr_start);
        f->address = (char *)malloc(len + 1);
        if (f->address) {
            memcpy(f->address, addr_start, len);
            f->address[len] = '\0';
        }
    }

    /* SYMBOL: either "in SYMBOL" or SYMBOL directly (e.g. #5g_main_...) */
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "in ", 3) == 0) {
        p += 3;
    }
    const char *sym_start = p;
    while (*p && strncmp(p, " at ", 4) != 0 && !(p[0] == ' ' && p[1] == '(')) p++;
    if (p > sym_start) {
        size_t len = (size_t)(p - sym_start);
        f->method = (char *)malloc(len + 1);
        if (f->method) {
            memcpy(f->method, sym_start, len);
            f->method[len] = '\0';
        }
    }

    /* (args) - capture including parens */
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '(') {
        const char *args_start = p;
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '(') depth++;
            else if (*p == ')') depth--;
            p++;
        }
        if (depth == 0 && p > args_start) {
            size_t len = (size_t)(p - args_start);
            f->params = (char *)malloc(len + 1);
            if (f->params) {
                memcpy(f->params, args_start, len);
                f->params[len] = '\0';
            }
        }
    }

    /* at file:line */
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "at ", 3) == 0) {
        p += 3;
        const char *file_start = p;
        const char *colon = NULL;
        while (*p && *p != '\n' && *p != '\r') {
            if (*p == ':' && colon == NULL && isdigit((unsigned char)p[1]))
                colon = p;
            p++;
        }
        if (colon && colon > file_start) {
            size_t file_len = (size_t)(colon - file_start);
            f->file = (char *)malloc(file_len + 1);
            if (f->file) {
                memcpy(f->file, file_start, file_len);
                f->file[file_len] = '\0';
            }
            f->line = (int)strtol(colon + 1, NULL, 10);
        }
    }
    return f;
}

static bool is_gdb_noise_line(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "[New LWP ", 9) == 0) return true;
    if (strncmp(p, "[Thread debugging using libthread_db enabled]", 47) == 0) return true;
    if (strncmp(p, "Using host libthread_db library ", 32) == 0) return true;
    return false;
}

static bool is_thread_header(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    return strncmp(p, "Thread ", 7) == 0;
}

GList *gdb_output_parse(FILE *in) {
    GList *result = NULL;
    char *line = NULL;
    size_t cap = 0;
    thread_info_t *current = NULL;

    while (getline(&line, &cap, in) > 0) {
        if (is_gdb_noise_line(line)) continue;

        if (is_thread_header(line)) {
            current = gdb_output_thread_info_parse(line);
            if (current) {
                result = g_list_append(result, current);
            }
            continue;
        }

        if (current && line[0] == '#') {
            stack_frame_t *f = gdb_output_stack_frame_parse(line);
            if (f) {
                current->frames = g_list_append(current->frames, f);
            }
        }
    }
    free(line);
    return result;
}

void gdb_output_highlight_fd(FILE *in, FILE *out, const stackdump_color_schema_t *color_schema) {
    const stackdump_color_schema_t *s = color_schema ? color_schema : &stackdump_color_schema_default;

    GList *threads = gdb_output_parse(in);
    thread_info_list_format(out, threads, s);
    thread_info_list_free(threads);
}

void gdb_output_highlight_file(const char *input_file, const char *output_file, const stackdump_color_schema_t *color_schema) {
    FILE *fp = fopen(input_file, "r");
    if (!fp) {
        fprintf(stderr, "stackdump: failed to open file: %s\n", input_file);
        return;
    }
    FILE *out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "stackdump: failed to write to file: %s\n", output_file);
        fclose(fp);
        return;
    }
    gdb_output_highlight_fd(fp, out, color_schema);
    fclose(fp);
    fclose(out);
}
