/*
 * Debug thread: keyboard monitor + timer.
 * Kbd: line-based console commands.
 *   bt / backtrace / stackdump  -> stackdump; -f FILE = output file, else stdout
 *   t / threads     -> top_threads
 *   f / list-fd     -> dump_fd
 *   w / timer [options] -> timer control; -f FILE = stackdump output file (default mktemp .dump)
 *
 * Timer: -s/--stackdump, -h/--health, -d/--dump-fd, -f FILE (stackdump path),
 *   -w/--interval <ms[unit]> (default 2s), -q/--quit (only way to stop).
 */

#define _POSIX_C_SOURCE 200809L

#include "dbgthread.h"

#include "format_backtrace.h"
#include "format_gdb.h"
#include "stackdump.h"

#define LOGGER mon_logger
#include "../util/_logging.h"
logger_t mon_logger;

#include "../util/args.h"

#include <ctype.h>
#include <strings.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STACKDUMP_FILE_MAX 260

#define TIMER_POLL_MS 100
#define TIMER_DEFAULT_INTERVAL_MS 2000

/* ANSI for interactive (colored) output */
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"

static int s_interactive = 0;

#define OUT_HDR(fmt) (s_interactive ? ANSI_BOLD ANSI_CYAN fmt ANSI_RESET : fmt)
#define OUT_HDR2(fmt) (s_interactive ? ANSI_GREEN fmt ANSI_RESET : fmt)

/* ---------------------------------------------------------------------------
 * top_threads / dump_fd
 * --------------------------------------------------------------------------- */

static void top_threads(void) {
#if defined(__linux__)
    DIR *dir = opendir("/proc/self/task");
    if (!dir) {
        logerror_fmt("top_threads: cannot open /proc/self/task: %s", strerror(errno));
        return;
    }

    long clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0) clk_tck = 100;

    fprintf(stderr, OUT_HDR("=== Threads (tid, comm, state, utime, stime) ===\n"));
    fprintf(stderr, "%-8s %-20s %4s %12s %12s\n", "TID", "COMM", "S", "utime(s)", "stime(s)");

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (!isdigit((unsigned char)ent->d_name[0])) continue;

        char path[256];
        char comm[64] = "";
        char state = '?';
        unsigned long utime = 0, stime = 0;

        snprintf(path, sizeof(path), "/proc/self/task/%s/comm", ent->d_name);
        FILE *f = fopen(path, "r");
        if (f) {
            if (fgets(comm, (int)sizeof(comm), f)) {
                size_t n = strlen(comm);
                if (n > 0 && comm[n - 1] == '\n') comm[n - 1] = '\0';
            }
            fclose(f);
        }

        snprintf(path, sizeof(path), "/proc/self/task/%s/stat", ent->d_name);
        f = fopen(path, "r");
        if (f) {
            char buf[512];
            if (fgets(buf, (int)sizeof(buf), f)) {
                char *r = strrchr(buf, ')');
                if (r && r[1] == ' ') {
                    r += 2;
                    if (sscanf(r, "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %lu %lu",
                               &state, &utime, &stime) >= 3) { (void)0; }
                }
            }
            fclose(f);
        }

        double ut = (double)utime / (double)clk_tck;
        double st = (double)stime / (double)clk_tck;
        fprintf(stderr, "%-8s %-20s %4c %12.3f %12.3f\n",
                ent->d_name, comm[0] ? comm : "-", state, ut, st);
    }

    closedir(dir);
    fprintf(stderr, OUT_HDR("=== end threads ===\n"));
#else
    fprintf(stderr, "top_threads: only Linux implemented\n");
#endif
}

static void dump_fd(void) {
#if defined(__linux__)
    DIR *dir = opendir("/proc/self/fd");
    if (!dir) {
        logerror_fmt("dump_fd: cannot open /proc/self/fd: %s", strerror(errno));
        return;
    }

    fprintf(stderr, OUT_HDR("=== Open file descriptors ===\n"));
    fprintf(stderr, "%-6s %-8s %12s %12s %s\n", "fd", "flags", "pos", "mnt_id", "path");

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (!isdigit((unsigned char)ent->d_name[0])) continue;

        int fd = atoi(ent->d_name);
        char path_fd[80], path_fdinfo[80];
        snprintf(path_fd, sizeof(path_fd), "/proc/self/fd/%s", ent->d_name);
        snprintf(path_fdinfo, sizeof(path_fdinfo), "/proc/self/fdinfo/%s", ent->d_name);

        char link[PATH_MAX];
        ssize_t n = readlink(path_fd, link, sizeof(link) - 1);
        if (n < 0) {
            snprintf(link, sizeof(link), "(readlink: %s)", strerror(errno));
            n = (ssize_t)strlen(link);
        } else {
            link[n] = '\0';
        }

        unsigned int flags_oct = 0;
        unsigned long long pos = 0;
        int mnt_id = -1;

        FILE *f = fopen(path_fdinfo, "r");
        if (f) {
            char line[256];
            while (fgets(line, (int)sizeof(line), f)) {
                if (strncmp(line, "pos:", 4) == 0)
                    sscanf(line + 4, "%llu", &pos);
                else if (strncmp(line, "flags:", 6) == 0)
                    sscanf(line + 6, "%o", &flags_oct);
                else if (strncmp(line, "mnt_id:", 7) == 0)
                    sscanf(line + 7, "%d", &mnt_id);
            }
            fclose(f);
        }

        fprintf(stderr, "%-6d 0%06o %12llu %12d %s\n",
                fd, flags_oct, (unsigned long long)pos, mnt_id, link);
    }

    closedir(dir);
    fprintf(stderr, OUT_HDR("=== end fd ===\n"));
#else
    fprintf(stderr, "dump_fd: only Linux implemented\n");
#endif
}

/* ---------------------------------------------------------------------------
 * Timer thread: small nanosleep steps to pick up interval/option changes
 * --------------------------------------------------------------------------- */

struct timer_context {
    pthread_t thread;
    volatile int quit;
    struct timer_options opts;
    unsigned long counter;
};

static struct timer_context *s_timer_ctx = NULL;
static pthread_mutex_t s_timer_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *timer_thread(void *arg) {
    struct timer_context *ctx = (struct timer_context *)arg;
    int accumulated_ms = 0;
    const struct timespec poll_ts = {
        .tv_sec = TIMER_POLL_MS / 1000,
        .tv_nsec = (long)(TIMER_POLL_MS % 1000) * 1000000,
    };

    logdebug_fmt("timer thread started, poll_interval=%d ms", TIMER_POLL_MS);

    while (!ctx->quit) {
        nanosleep(&poll_ts, NULL);
        if (ctx->quit) break;

        int interval_ms;
        int do_stackdump, do_health, do_dump_fd;

        pthread_mutex_lock(&s_timer_mutex);
        interval_ms = ctx->opts.interval_ms;
        do_stackdump = ctx->opts.do_stackdump;
        do_health = ctx->opts.do_health;
        do_dump_fd = ctx->opts.do_dump_fd;
        pthread_mutex_unlock(&s_timer_mutex);

        if (interval_ms <= 0)
            interval_ms = TIMER_DEFAULT_INTERVAL_MS;

        accumulated_ms += TIMER_POLL_MS;
        if (accumulated_ms < interval_ms)
            continue;

        accumulated_ms = 0;
        ctx->counter++;

        if (do_health) {
            if (s_interactive)
                fprintf(stderr, OUT_HDR2("timer triggered %lu\n"), (unsigned long)ctx->counter);
            else
                fprintf(stderr, "timer triggered %lu\n", (unsigned long)ctx->counter);
        }

        if (do_stackdump && ctx->opts.stackdump_file[0] != '\0') {
            logdebug_fmt("timer: stackdump (count %lu) -> %s", (unsigned long)ctx->counter, ctx->opts.stackdump_file);
            stackdump(ctx->opts.stackdump_file, &stackdump_color_schema_default);
        }

        if (do_dump_fd) {
            logdebug_fmt("timer: dump_fd (count %lu)", (unsigned long)ctx->counter);
            dump_fd();
        }
    }

    logdebug("timer thread exiting");
    return NULL;
}

void *start_timer_thread(const struct timer_options *opts) {
    if (!opts) return NULL;

    pthread_mutex_lock(&s_timer_mutex);

    if (s_timer_ctx) {
        /* Already running: only update parameters */
        int interval = opts->interval_ms > 0 ? opts->interval_ms : TIMER_DEFAULT_INTERVAL_MS;
        s_timer_ctx->opts.interval_ms = interval;
        s_timer_ctx->opts.do_stackdump = opts->do_stackdump ? 1 : 0;
        s_timer_ctx->opts.do_health = opts->do_health ? 1 : 0;
        s_timer_ctx->opts.do_dump_fd = opts->do_dump_fd ? 1 : 0;
        if (opts->stackdump_file[0] != '\0')
            snprintf(s_timer_ctx->opts.stackdump_file, sizeof(s_timer_ctx->opts.stackdump_file), "%s", opts->stackdump_file);
        loginfo_fmt("timer params updated: interval=%d ms stackdump=%d health=%d dump_fd=%d",
                    interval, s_timer_ctx->opts.do_stackdump,
                    s_timer_ctx->opts.do_health, s_timer_ctx->opts.do_dump_fd);
        pthread_mutex_unlock(&s_timer_mutex);
        return s_timer_ctx;
    }

    struct timer_context *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        pthread_mutex_unlock(&s_timer_mutex);
        logerror_fmt("timer: calloc context failed");
        return NULL;
    }

    ctx->quit = 0;
    ctx->counter = 0;
    ctx->opts.interval_ms = opts->interval_ms > 0 ? opts->interval_ms : TIMER_DEFAULT_INTERVAL_MS;
    ctx->opts.do_stackdump = opts->do_stackdump ? 1 : 0;
    ctx->opts.do_health = opts->do_health ? 1 : 0;
    ctx->opts.do_dump_fd = opts->do_dump_fd ? 1 : 0;
    ctx->opts.stackdump_file[0] = '\0';
    if (opts->stackdump_file[0] != '\0')
        snprintf(ctx->opts.stackdump_file, sizeof(ctx->opts.stackdump_file), "%s", opts->stackdump_file);
    else if (ctx->opts.do_stackdump) {
        char path[] = "/tmp/stack.XXXXXX.dump";
        int fd = mkstemp(path);
        if (fd >= 0) {
            close(fd);
            unlink(path);
            snprintf(ctx->opts.stackdump_file, sizeof(ctx->opts.stackdump_file), "%s.dump", path);
            fprintf(stderr, "timer: stackdump file %s\n", ctx->opts.stackdump_file);
        }
    }

    if (pthread_create(&ctx->thread, NULL, timer_thread, ctx) != 0) {
        free(ctx);
        pthread_mutex_unlock(&s_timer_mutex);
        logerror_fmt("timer: pthread_create failed: %s", strerror(errno));
        return NULL;
    }

    s_timer_ctx = ctx;
    loginfo_fmt("timer started: interval=%d ms stackdump=%d health=%d dump_fd=%d",
                ctx->opts.interval_ms, ctx->opts.do_stackdump,
                ctx->opts.do_health, ctx->opts.do_dump_fd);
    pthread_mutex_unlock(&s_timer_mutex);
    return ctx;
}

void stop_timer_thread(void *context) {
    (void)context;
    pthread_mutex_lock(&s_timer_mutex);
    if (s_timer_ctx) {
        s_timer_ctx->quit = 1;
        pthread_mutex_unlock(&s_timer_mutex);
        pthread_join(s_timer_ctx->thread, NULL);
        pthread_mutex_lock(&s_timer_mutex);
        loginfo_fmt("timer stopped (counter was %lu)", (unsigned long)s_timer_ctx->counter);
        free(s_timer_ctx);
        s_timer_ctx = NULL;
    }
    pthread_mutex_unlock(&s_timer_mutex);
}

/* Print current timer status to out. */
static void print_timer_status(FILE *out) {
    pthread_mutex_lock(&s_timer_mutex);
    if (s_timer_ctx) {
        fprintf(out, "timer: running\n");
        fprintf(out, "  interval_ms   %d\n", s_timer_ctx->opts.interval_ms);
        fprintf(out, "  stackdump     %d\n", s_timer_ctx->opts.do_stackdump);
        if (s_timer_ctx->opts.stackdump_file[0] != '\0')
            fprintf(out, "  stackdump_file %s\n", s_timer_ctx->opts.stackdump_file);
        fprintf(out, "  health        %d\n", s_timer_ctx->opts.do_health);
        fprintf(out, "  dump_fd       %d\n", s_timer_ctx->opts.do_dump_fd);
        fprintf(out, "  counter       %lu\n", (unsigned long)s_timer_ctx->counter);
    } else {
        fprintf(out, "timer: stopped\n");
    }
    pthread_mutex_unlock(&s_timer_mutex);
}

static void (*s_exit_cb)(void *userdata) = NULL;
static void *s_exit_userdata = NULL;

void dbgthread_set_exit_callback(void (*cb)(void *userdata), void *userdata) {
    s_exit_cb = cb;
    s_exit_userdata = userdata;
}

/* Parse interval value: "2000", "2s", "2000ms" -> ms. Returns -1 on error. */
static int parse_interval_ms(const char *s) {
    unsigned long val;
    char *end;
    if (!s || !*s) return -1;
    val = strtoul(s, &end, 10);
    if (end == s) return -1;
    if (val > INT_MAX) return -1;
    for (; *end == ' ' || *end == '\t'; end++) ;
    if (*end == 's' || *end == 'S') {
        if (val > (unsigned long)INT_MAX / 1000) return -1;
        return (int)(val * 1000);
    }
    if (*end == 'm' && (end[1] == 's' || end[1] == 'S'))
        return (int)val;
    if (*end == '\0')
        return (int)val;
    return (int)val;
}

/* Parse "timer" or "w" command args; start/update or stop timer. */
static void run_timer_cmd(int argc, char **argv) {
    struct timer_options opts = {
        .interval_ms = TIMER_DEFAULT_INTERVAL_MS,
        .do_stackdump = 0,
        .do_health = 0,
        .do_dump_fd = 0,
        .stackdump_file = { 0 },
    };
    int quit = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stackdump") == 0) {
            opts.do_stackdump = 1;
            continue;
        }
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--health") == 0) {
            opts.do_health = 1;
            continue;
        }
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dump-fd") == 0) {
            opts.do_dump_fd = 1;
            continue;
        }
        if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 < argc) {
                snprintf(opts.stackdump_file, sizeof(opts.stackdump_file), "%s", argv[i + 1]);
                i++;
            }
            continue;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quit") == 0) {
            quit = 1;
            continue;
        }
        if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--interval") == 0) {
            if (i + 1 < argc) {
                int ms = parse_interval_ms(argv[i + 1]);
                if (ms > 0) opts.interval_ms = ms;
                i++;
            }
            continue;
        }
        if (argv[i][0] != '\0' && isdigit((unsigned char)argv[i][0])) {
            int ms = parse_interval_ms(argv[i]);
            if (ms > 0) opts.interval_ms = ms;
        }
    }

    if (quit) {
        stop_timer_thread(NULL);
        if (s_interactive)
            fprintf(stderr, OUT_HDR2("timer stopped\n"));
        else
            fprintf(stderr, "timer stopped\n");
        return;
    }

    if (!opts.do_stackdump && !opts.do_health && !opts.do_dump_fd)
        opts.do_health = 1;

    if (start_timer_thread(&opts)) {
        if (s_interactive)
            fprintf(stderr, OUT_HDR2("timer: interval=%d ms stackdump=%d health=%d dump_fd=%d\n"),
                    opts.interval_ms, opts.do_stackdump, opts.do_health, opts.do_dump_fd);
        else
            fprintf(stderr, "timer: interval=%d ms stackdump=%d health=%d dump_fd=%d\n",
                    opts.interval_ms, opts.do_stackdump, opts.do_health, opts.do_dump_fd);
    } else {
        logerror_fmt("timer start/update failed");
        fprintf(stderr, "timer start failed\n");
    }
}

/* ---------------------------------------------------------------------------
 * Keyboard monitor: readline-based commands
 * --------------------------------------------------------------------------- */

static pthread_t g_dbg_thread;
static volatile int g_kbd_quit;

static void trim_line(char *line) {
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || (unsigned char)line[n - 1] == ' '))
        line[--n] = '\0';
    char *start = line;
    while (*start == ' ' || *start == '\t') start++;
    if (start != line)
        memmove(line, start, strlen(start) + 1);
}


/** Return 1 if arg matches cmd (case-insensitive). */
static int arg_is(const char *arg, const char *cmd) {
    return arg && strcasecmp(arg, cmd) == 0;
}

static void run_bt_backtrace_stackdump(int argc, char **argv) {
    const char *out_file = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            out_file = argv[i + 1];
            break;
        }
    }

    char base_path[STACKDUMP_FILE_MAX];
    int use_temp = 0;
    if (out_file && out_file[0] != '\0') {
        snprintf(base_path, sizeof(base_path), "%s", out_file);
    } else {
        snprintf(base_path, sizeof(base_path), "/tmp/stackdump.XXXXXX");
        int fd = mkstemp(base_path);
        if (fd < 0) {
            logerror_fmt("bt: mkstemp failed: %s", strerror(errno));
            fprintf(stderr, "bt: mkstemp failed\n");
            return;
        }
        close(fd);
        use_temp = 1;
    }

    stackdump(base_path, false);
    {
        char path[STACKDUMP_FILE_MAX + 16];
        snprintf(path, sizeof(path), "%s.gdb", base_path);
        FILE *f = fopen(path, "r");
        if (f) {
            gdb_output_highlight_fd(f, stdout, NULL);
            fclose(f);
        } else {
            snprintf(path, sizeof(path), "%s.bt", base_path);
            f = fopen(path, "r");
            if (f) {
                    backtrace_highlight_fd(f, stdout, NULL);
                fclose(f);
            }
        }
    }
    if (use_temp) {
        char tmp[STACKDUMP_FILE_MAX + 16];
        unlink(base_path);
        snprintf(tmp, sizeof(tmp), "%s.gdb", base_path);
        unlink(tmp);
        snprintf(tmp, sizeof(tmp), "%s.bt", base_path);
        unlink(tmp);
    }
}

static void run_command(char *line) {
    trim_line(line);
    if (!*line) return;

    logdebug_fmt("kbd command: [%s]", line);

    int argc = 0;
    char **argv = NULL;
    if (args_parse(NULL, NULL, NULL, line, &argc, &argv) != 0) {
        fprintf(stderr, "? parse error (unmatched quotes?)\n");
        return;
    }
    if (argc == 0) return;

    if (arg_is(argv[0], "bt") || arg_is(argv[0], "backtrace") || arg_is(argv[0], "stackdump")) {
        run_bt_backtrace_stackdump(argc, argv);
        goto out;
    }
    if (arg_is(argv[0], "t") || arg_is(argv[0], "threads")) {
        top_threads();
        goto out;
    }
    if (arg_is(argv[0], "f") || arg_is(argv[0], "list-fd")) {
        dump_fd();
        goto out;
    }
    if (arg_is(argv[0], "w") || arg_is(argv[0], "timer")) {
        run_timer_cmd(argc, argv);
        goto out;
    }
    if (arg_is(argv[0], "h") || arg_is(argv[0], "help")) {
        fprintf(stderr,
            "  bt, backtrace, stackdump [-f FILE]  dump backtrace (-f FILE else stdout)\n"
            "  t, threads      list threads (tid, comm, utime, stime)\n"
            "  f, list-fd      list open fds with path and pos\n"
            "  w, timer [opts] timer: -s -h -d -f FILE -w <ms> -q (-f = stackdump file)\n"
            "  \\s, status      print status (timer settings etc.)\n"
            "  do [action]     run UI action; no arg = list actions\n"
            "  h, help         this help\n"
            "  exit            exit the app\n"
            "  Ctrl-D (EOF)    stop kbd thread\n");
        goto out;
    }
    if ((argv[0][0] == '\\' && argv[0][1] == 's' && argv[0][2] == '\0') || arg_is(argv[0], "status")) {
        fprintf(stderr, OUT_HDR("=== status ===\n"));
        print_timer_status(stderr);
        fprintf(stderr, OUT_HDR("=== end status ===\n"));
        goto out;
    }
    if (arg_is(argv[0], "do")) {
        fprintf(stderr, "dbgthread: UI actions are not available in this build.\n");
        goto out;
    }
    if (arg_is(argv[0], "exit")) {
        if (argv) args_free(argv, argc);
        if (s_exit_cb)
            s_exit_cb(s_exit_userdata);
        else
            exit(0);
        return;
    }

    fprintf(stderr, "? unknown command (h/help for help)\n");
out:
    if (argv) args_free(argv, argc);
}

static void *kbd_monitor_thread(void *arg) {
    (void)arg;
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;

    loginfo("kbd monitor: interactive mode (ANSI coloring on)");
    while (!g_kbd_quit) {
        fputs("Dbg> ", stderr);
        fflush(stderr);
        n = getline(&line, &cap, stdin);
        if (n <= 0) {
            /* Ctrl-D / EOF: stop kbd thread */
            g_kbd_quit = 1;
            break;
        }
        run_command(line);
    }

    free(line);
    return NULL;
}

void *start_dbg_thread(void) {
    g_kbd_quit = 0;
    s_interactive = 1;
    stackdump_set_interactive(1);
    if (pthread_create(&g_dbg_thread, NULL, kbd_monitor_thread, NULL) != 0) {
        s_interactive = 0;
        stackdump_set_interactive(0);
        logerror_fmt("dbgthread: pthread_create failed: %s", strerror(errno));
        return NULL;
    }
    loginfo("kbd thread started");
    return &g_dbg_thread;
}

void stop_dbg_thread(void *context) {
    (void)context;
    g_kbd_quit = 1;
    s_interactive = 0;
    stackdump_set_interactive(0);
    pthread_join(g_dbg_thread, NULL);
    loginfo("kbd thread stopped");
}
