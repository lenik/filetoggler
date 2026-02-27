#ifndef FORMAT_BACKTRACE_H
#define FORMAT_BACKTRACE_H

#include "stackdump.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Parse backtrace output (e.g. from stackdump_current_thread) into list of thread_info_t. */
GList *backtrace_output_parse(FILE *in);

/** Highlight backtrace from stream; if color_schema is NULL, use stackdump_color_schema_default. */
void backtrace_highlight_fd(FILE *in, FILE *out, const stackdump_color_schema_t *color_schema);

void backtrace_highlight_file(const char *input_file, const char *output_file, const stackdump_color_schema_t *color_schema);

#ifdef __cplusplus
}
#endif

#endif /* FORMAT_BACKTRACE_H */
