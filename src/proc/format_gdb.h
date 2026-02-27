#ifndef FORMAT_GDB_H
#define FORMAT_GDB_H

#include "stackdump.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Parse GDB "Thread N (...)" header line into thread_info_t (frames list empty). Caller fills frames. */
thread_info_t *gdb_output_thread_info_parse(const char *line);

/** Parse GDB/#N frame line into stack_frame_t. */
stack_frame_t *gdb_output_stack_frame_parse(const char *line);

/** Parse full GDB "thread apply all bt" output into list of thread_info_t. */
GList *gdb_output_parse(FILE *in);

/** Highlight GDB output from stream; if color_schema is NULL, use stackdump_color_schema_default. */
void gdb_output_highlight_fd(FILE *in, FILE *out, const stackdump_color_schema_t *color_schema);

void gdb_output_highlight_file(const char *input_file, const char *output_file, const stackdump_color_schema_t *color_schema);

#ifdef __cplusplus
}
#endif

#endif /* FORMAT_GDB_H */
