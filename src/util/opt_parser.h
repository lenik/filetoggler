#ifndef OPT_PARSER_H
#define OPT_PARSER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Option parser state structure.
 * This replaces the global state used by getopt/getopt_long.
 */
typedef struct {
    int optind;      // Index of next element to be processed in argv
    int optopt;      // Option character that caused an error
    int opterr;      // Error reporting flag (0 = suppress, 1 = enable)
    const char* optarg;  // Argument associated with option
    // Internal state for short option parsing
    int short_opt_pos;  // Position within current short option string (e.g., "-abc" position)
    char* current_arg;  // Current argument being processed (for short options)
} opt_parser_t;

// Include getopt.h for struct option definition
#include <getopt.h>

/**
 * Option structure for long options.
 * Uses the standard struct option from getopt.h for compatibility.
 */

/**
 * Initialize an option parser state.
 * 
 * @param parser Parser state to initialize
 */
void opt_parser_init(opt_parser_t* parser);

/**
 * Parse command-line options (similar to getopt_long).
 * This function is reentrant and doesn't use global state.
 * 
 * @param parser Parser state (must be initialized with opt_parser_init)
 * @param argc Argument count
 * @param argv Argument vector
 * @param optstring String of short options (e.g., "abc:def:" means a, b, c (with arg), d, e, f (with arg))
 * @param longopts Array of long option structures (terminated with {0,0,0,0})
 * @param longindex If not NULL, set to index of matched long option
 * @return Option character, or -1 when done, or '?' for unknown option
 * 
 * Usage example:
 *   opt_parser_t parser;
 *   opt_parser_init(&parser);
 *   while ((opt = opt_parse_long(&parser, argc, argv, "abc:", long_options, NULL)) != -1) {
 *       switch (opt) {
 *           case 'a': ... break;
 *           case 'b': ... break;
 *           case 'c': printf("arg: %s\n", parser.optarg); break;
 *           case '?': ... break;
 *       }
 *   }
 *   // Non-option arguments start at parser.optind
 */
int opt_parse_long(opt_parser_t* parser, int argc, char* const argv[],
                   const char* optstring, const struct option* longopts, int* longindex);

/**
 * Parse command-line options (short options only, similar to getopt).
 * 
 * @param parser Parser state (must be initialized with opt_parser_init)
 * @param argc Argument count
 * @param argv Argument vector
 * @param optstring String of short options
 * @return Option character, or -1 when done, or '?' for unknown option
 */
int opt_parse(opt_parser_t* parser, int argc, char* const argv[], const char* optstring);

#ifdef __cplusplus
}
#endif

#endif /* OPT_PARSER_H */

