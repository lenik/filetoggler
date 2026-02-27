#ifndef DBGTHREAD_H
#define DBGTHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

/** Timer options; extend for future options. */
struct timer_options {
    int interval_ms;    /**< Wake interval in ms; 0 or negative = use default (2000). */
    int do_stackdump;   /**< 1 = run stackdump each interval. */
    int do_health;      /**< 1 = print "timer triggered <counter>". */
    int do_dump_fd;     /**< 1 = run dump_fd each interval. */
    /** Stackdump output path; if empty, use mkstemp with .dump (and print the path). */
    char stackdump_file[260];
};

/** Start timer thread if not running (with opts), or only update parameters if already running.
 *  Does not stop the timer; use stop_timer_thread for that (e.g. timer -q). */
void *start_timer_thread(const struct timer_options *opts);
/** Stop the timer thread. */
void stop_timer_thread(void *context);

/** Keyboard monitor thread (stdin, line-based). Commands:
 *  bt/backtrace, t/threads, f/list-fd, w/timer, h/help, \\s/status, do [action], exit.
 *  Ctrl-D (EOF) stops the kbd thread. */
void *start_dbg_thread(void);
void stop_dbg_thread(void *context);

/** Set callback for "exit" command. If set, called from kbd thread; else exit(0). */
void dbgthread_set_exit_callback(void (*cb)(void *userdata), void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* DBGTHREAD_H */
