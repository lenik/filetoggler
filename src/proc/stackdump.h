#ifndef STACKDUMP_H
#define STACKDUMP_H

#include <glib.h>

#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int g_interactive;

/** Color schema for highlighted stackdump output (ANSI codes). */
typedef struct stackdump_color_schema {
    const char *reset;
    const char *header;
    const char *frame_index;
    const char *symbol;
    const char *params;
    const char *location;
} stackdump_color_schema_t;

extern const stackdump_color_schema_t stackdump_color_schema_default;

/** Parsed stack frame: #N [0x... ]in method (params) at file:line */
typedef struct stack_frame_t {
    int frame_number;
    char *address;   /**< 0x... hex string or NULL */
    char *method;   /**< symbol name */
    char *params;   /**< (args) including parens or NULL */
    char *file;     /**< path or NULL */
    int line;       /**< 0 if unknown */
    char *raw_line; /**< original line for fallback display */
} stack_frame_t;

/** Parsed thread: header line + list of frames */
typedef struct thread_info_t {
    int thread_id;
    char *name;     /**< from "name" in thread header */
    int lwp;        /**< LWP id, 0 if unknown */
    GList *frames; /**< GList of stack_frame_t* */
} thread_info_t;

void stack_frame_free(stack_frame_t *f);
void thread_info_free(thread_info_t *t);
void thread_info_list_free(GList *threads);

bool is_thread_included(thread_info_t *t);

void thread_info_list_format(FILE *out, GList *threads, const stackdump_color_schema_t *s);
void thread_info_format(FILE *out, thread_info_t *t, const stackdump_color_schema_t *s);
void stack_frame_format(FILE *out, stack_frame_t *f, const stackdump_color_schema_t *s);

void stackdump(const char *file, const stackdump_color_schema_t *s);
/** Dump backtrace of target process (e.g. crashed parent from signal handler child). */
void stackdump_pid(int target_pid, const char *file, const stackdump_color_schema_t *s);

void stackdump_current_thread(const char *file);

/** Install handler for SIGSEGV, SIGABRT, SIGBUS, SIGFPE to auto-capture stackdump to /tmp/segfault.<pid>.dump */
void stackdump_install_crash_handler(const stackdump_color_schema_t *s);

/** Tell stackdump whether the app is in interactive (Dbg> prompt) mode. */
void stackdump_set_interactive(int interactive);

#ifdef __cplusplus
}
#endif

#endif // STACKDUMP_H
