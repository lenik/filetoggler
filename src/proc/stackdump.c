#include "stackdump.h"

#include "format_backtrace.h"
#include "format_gdb.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_GRAY    "\033[90m"

int g_interactive = 0;

const stackdump_color_schema_t stackdump_color_schema_default = {
    .reset       = ANSI_RESET,
    .header      = ANSI_BOLD ANSI_CYAN,
    .frame_index = ANSI_CYAN,
    .symbol      = ANSI_GREEN,
    .params      = ANSI_GRAY,
    .location    = ANSI_YELLOW,
};

void stack_frame_free(stack_frame_t *f) {
    if (!f) return;
    free(f->address);
    free(f->method);
    free(f->params);
    free(f->file);
    free(f->raw_line);
    free(f);
}

void thread_info_free(thread_info_t *t) {
    if (!t) return;
    free(t->name);
    if (t->frames) {
        for (GList *l = t->frames; l; l = l->next)
            stack_frame_free((stack_frame_t *)l->data);
        g_list_free(t->frames);
    }
    free(t);
}

void thread_info_list_free(GList *threads) {
    for (GList *l = threads; l; l = l->next)
        thread_info_free((thread_info_t *)l->data);
    g_list_free(threads);
}

static bool thread_has_method(thread_info_t *t, const char *method_substr) {
    if (!t || !t->frames || !method_substr) return false;
    for (GList *l = t->frames; l; l = l->next) {
        stack_frame_t *f = (stack_frame_t *)l->data;
        if (f->method && strstr(f->method, method_substr)) return true;
        if (f->raw_line && strstr(f->raw_line, method_substr)) return true;
    }
    return false;
}

bool is_thread_included(thread_info_t *t) {
    if (!t) return false;
    const char *name = t->name; 
    if (t->name) {
        if (strcmp(name, "pool-0") == 0) return false;
        if (strcmp(name, "pool-1") == 0) return false;
        if (strcmp(name, "pool-spawner") == 0) return false;
        if (strcmp(name, "gmain") == 0) return false;
        if (strcmp(name, "gdbus") == 0) return false;
        if (strncmp(name, "[pango]", 7) == 0) return false;
        if (strcmp(name, "libusb_event") == 0) return false;
    }
    if (thread_has_method(t, "HttpDaemon::serverLoop")) return false;
    return true;
}

void thread_info_list_format(FILE *out, GList *threads, const stackdump_color_schema_t *s) {
    int thread_index = 0;
    for (GList *l = threads; l; l = l->next) {
        thread_info_t *t = (thread_info_t *)l->data;
        if (!is_thread_included(t)) continue;
        if (thread_index++ > 0)
            fputc('\n', out);
        thread_info_format(out, t, s);
        for (GList *fr = t->frames; fr; fr = fr->next) {
            fputs("    ", out);
            stack_frame_format(out, (stack_frame_t *)fr->data, s);
        }
    }
}

void thread_info_format(FILE *out, thread_info_t *t, const stackdump_color_schema_t *s) {
    /* Reconstruct header line for display: "Thread N (LWP M) \"name\"" or use raw if we stored it. We don't store raw header; so print minimal. */
    char buf[256];
    if (t->lwp > 0)
        snprintf(buf, sizeof(buf), "Thread %d (LWP %d) \"%s\"\n", t->thread_id, t->lwp, t->name ? t->name : "?");
    else
        snprintf(buf, sizeof(buf), "Thread %d \"%s\"\n", t->thread_id, t->name ? t->name : "?");
    fputs(s->header, out);
    fputs(buf, out);
    fputs(s->reset, out);
}

void stack_frame_format(FILE *out, stack_frame_t *f, const stackdump_color_schema_t *s) {
    const char *RESET = s->reset;
    const char *FRAME_INDEX = s->frame_index;
    const char *SYMBOL = s->symbol;
    const char *PARAMS = s->params;
    const char *LOCATION = s->location;

    /* #N - align with raw: #40x... or #5g_main_... */
    fputs(FRAME_INDEX, out);
    fprintf(out, "#%d", f->frame_number);
    fputs(RESET, out);

    if (f->address && f->address[0]) {
        fputc(' ', out);
        fputs(f->address, out);
        fputs(" in ", out);
    }

    if (f->method && f->method[0]) {
        fputs(SYMBOL, out);
        fputs(f->method, out);
        fputs(RESET, out);
    }

    if (f->params && f->params[0]) {
        fputc(' ', out);
        fputs(PARAMS, out);
        fputs(f->params, out);
        fputs(RESET, out);
    }

    if (f->file && f->file[0]) {
        fputs(" at ", out);
        fputs(LOCATION, out);
        if (f->line > 0)
            fprintf(out, "%s:%d", f->file, f->line);
        else
            fputs(f->file, out);
        fputs(RESET, out);
    }

    fputc('\n', out);
}

#if defined(__linux__)
/** Run gdb against target_pid and write backtrace to file (base path). Returns 0 if gdb succeeded. */
static int stackdump_gdb_pid(pid_t target_pid, const char *file, stackdump_color_schema_t *s) {
    char gdb_output_file[PATH_MAX];
    char gdb_output_file_highlighted[PATH_MAX];
    char backtrace_output_file[PATH_MAX];
    char backtrace_output_file_highlighted[PATH_MAX];
    snprintf(gdb_output_file, sizeof(gdb_output_file), "%s.gdb", file);
    snprintf(gdb_output_file_highlighted, sizeof(gdb_output_file_highlighted), "%s.gdb.colored", file);
    snprintf(backtrace_output_file, sizeof(backtrace_output_file), "%s.bt", file);
    snprintf(backtrace_output_file_highlighted, sizeof(backtrace_output_file_highlighted), "%s.bt.colored", file);

    pid_t child = fork();
    if (child == 0) {
        fprintf(stderr, "stackdump: create gdb output file: %s\n", gdb_output_file);
        int fd = open(gdb_output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(STDOUT_FILENO, STDERR_FILENO);
            close(fd);
        }
        char pid_str[32];
        snprintf(pid_str, sizeof(pid_str), "%d", (int)target_pid);
        char *args[] = {
            "gdb", "-p", pid_str, "-batch",
            "-ex", "set pagination off",
            "-ex", "thread apply all bt",
            "-ex", "quit",
            NULL,
        };
        execv("/usr/bin/gdb", args);
        _exit(127);
    }
    if (child < 0) {
        fprintf(stderr, "stackdump: failed to fork: %s\n", strerror(errno));
        return -1;
    }
    int status;
    waitpid(child, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "stackdump: gdb failed: %d\n", WEXITSTATUS(status));
        stackdump_current_thread(backtrace_output_file);
        if (s) {
            backtrace_highlight_file(backtrace_output_file, backtrace_output_file_highlighted, NULL);
        }
        fprintf(stderr, "stackdump_current_thread: %s output: %s\n",
            s ? "highlighted" : "plain",
            s ? backtrace_output_file_highlighted : backtrace_output_file);
        return -1;
    }
    if (s) {
        gdb_output_highlight_file(gdb_output_file, gdb_output_file_highlighted, NULL);
    }
    fprintf(stderr, "stackdump: gdb %s output: %s\n",
        s ? "highlighted" : "plain",
        s ? gdb_output_file_highlighted : gdb_output_file);
    return 0;
}
#endif

void stackdump(const char *file, const stackdump_color_schema_t *s) {
#if defined(__linux__)
    (void)stackdump_gdb_pid(getpid(), file, s);
#else
    (void)file;
    (void)s;
    FILE *out = fopen(file, "w");
    if (out) fclose(out);
    fprintf(stderr, "stackdump: only Linux multi-thread dump implemented\n");
#endif
}

void stackdump_pid(int target_pid, const char *file, const stackdump_color_schema_t *s) {
#if defined(__linux__)
    (void)stackdump_gdb_pid((pid_t)target_pid, file, s);
#else
    (void)target_pid;
    (void)file;
    (void)s;
    fprintf(stderr, "stackdump_pid: only Linux implemented\n");
#endif
}

#if defined(__linux__)
#define CRASH_DUMP_PATH_MAX 264
static char g_crash_dump_path[CRASH_DUMP_PATH_MAX];
static stackdump_color_schema_t *g_color_schema = NULL;

static void crash_handler(int sig) {
    pid_t crashed_pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        if (g_interactive) {
            /* In interactive mode: dump to temp file, then print highlighted stacktrace to stdout. */
            char gdb_file[PATH_MAX];
            (void)stackdump_gdb_pid(crashed_pid, g_crash_dump_path, g_color_schema);
            snprintf(gdb_file, sizeof(gdb_file), "%s.gdb", g_crash_dump_path);
            FILE *in = fopen(gdb_file, "r");
            if (in) {
                gdb_output_highlight_fd(in, stdout, g_color_schema);
                fclose(in);
            }
        } else {
            /* Non-interactive: keep existing behavior, save to files only. */
            stackdump_pid(crashed_pid, g_crash_dump_path, g_color_schema);
        }
        _exit(0);
    }
    if (child > 0) {
        int status;
        waitpid(child, &status, 0);
    }
    _exit(128 + sig);
}

void stackdump_install_crash_handler(const stackdump_color_schema_t *s) {
    g_color_schema = s;
    snprintf(g_crash_dump_path, sizeof(g_crash_dump_path), "/tmp/segfault.%d.dump", (int)getpid());
    struct sigaction sa = {
        .sa_handler = crash_handler,
        .sa_flags = 0,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    fprintf(stderr, "stackdump: crash handler installed -> %s\n", g_crash_dump_path);
}

void stackdump_set_interactive(int interactive) {
    g_interactive = interactive ? 1 : 0;
}
#else
void stackdump_install_crash_handler(void) {
    (void)0;
}
#endif

void stackdump_current_thread(const char *file) {
    FILE *out = fopen(file, "w");
    if (!out) {
        return;
    }
#if defined(__linux__)
    fprintf(out, "=== Thread (current) ===\n");
    void* array[64];
    int n = backtrace(array, 64);
    char** strs = backtrace_symbols(array, n);
    if (strs) {
        for (int i = 0; i < n; ++i)
            fprintf(out, "%s\n", strs[i]);
        free(strs);
    }
#else
    fprintf(stderr, "stackdump_current_thread: only Linux multi-thread dump implemented\n");
#endif
    fclose(out);
}

