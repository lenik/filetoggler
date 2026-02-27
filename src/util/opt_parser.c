#include "opt_parser.h"
#include <string.h>
#include <stdio.h>

void opt_parser_init(opt_parser_t* parser) {
    if (parser) {
        parser->optind = 1;
        parser->optopt = 0;
        parser->opterr = 1;
        parser->optarg = NULL;
        parser->short_opt_pos = 0;
        parser->current_arg = NULL;
    }
}

static int find_long_option(const struct option* longopts, const char* name, int* index) {
    if (!longopts || !name) return -1;
    
    for (int i = 0; longopts[i].name; i++) {
        if (strcmp(longopts[i].name, name) == 0) {
            if (index) *index = i;
            return i;
        }
    }
    return -1;
}

static int find_short_option(const char* optstring, char c) {
    if (!optstring) return -1;
    
    const char* p = optstring;
    while (*p) {
        if (*p == c) {
            // Check if next character is ':'
            if (p[1] == ':') {
                if (p[2] == ':') {
                    return 2; // optional argument
                }
                return 1; // required argument
            }
            return 0; // no argument
        }
        p++;
    }
    return -1;
}

int opt_parse_long(opt_parser_t* parser, int argc, char* const argv[],
                   const char* optstring, const struct option* longopts, int* longindex) {
    if (!parser || !argv || argc < 1) {
        return -1;
    }
    
    // Reset optarg
    parser->optarg = NULL;
    
    // Check if we've processed all arguments
    if (parser->optind >= argc) {
        return -1;
    }
    
    char* arg = argv[parser->optind];
    
    // Skip non-option arguments (those that don't start with '-')
    if (!arg || arg[0] != '-') {
        return -1;
    }
    
    // Handle "--" (end of options)
    if (strcmp(arg, "--") == 0) {
        parser->optind++;
        return -1;
    }
    
    // Handle long options (--option)
    if (arg[1] == '-' && arg[2] != '\0') {
        char* option_name = arg + 2;
        char* equals = strchr(option_name, '=');
        if (equals) {
            *equals = '\0'; // Temporarily null-terminate
        }
        
        int long_idx = -1;
        int found = find_long_option(longopts, option_name, &long_idx);
        
        if (found >= 0) {
            const struct option* opt = &longopts[long_idx];
            
            if (longindex) {
                *longindex = long_idx;
            }
            
            parser->optind++;
            
            // Handle argument
            if (opt->has_arg == 1) { // required_argument
                if (equals) {
                    parser->optarg = equals + 1;
                } else if (parser->optind < argc) {
                    parser->optarg = argv[parser->optind];
                    parser->optind++;
                } else {
                    if (parser->opterr) {
                        fprintf(stderr, "%s: option '--%s' requires an argument\n", argv[0], option_name);
                    }
                    parser->optopt = 0;
                    if (equals) {
                        *equals = '='; // Restore
                    }
                    return '?';
                }
            } else if (opt->has_arg == 2) { // optional_argument
                if (equals) {
                    parser->optarg = equals + 1;
                }
            }
            
            if (equals) {
                *equals = '='; // Restore
            }
            
            if (opt->flag) {
                *opt->flag = opt->val;
                return 0;
            }
            return opt->val;
        } else {
            // Unknown long option
            if (parser->opterr) {
                fprintf(stderr, "%s: unrecognized option '--%s'\n", argv[0], option_name);
            }
            parser->optopt = 0;
            parser->optind++;
            if (equals) {
                *equals = '='; // Restore
            }
            return '?';
        }
    }
    
    // Handle short options (-a, -abc, etc.)
    if (arg[1] != '\0' && arg[1] != '-') {
        // Check if we're continuing with a previous short option argument
        if (parser->current_arg != arg) {
            // New argument, reset position
            parser->current_arg = arg;
            parser->short_opt_pos = 1; // Start after '-'
        }
        
        // Check if we've finished this argument
        if (arg[parser->short_opt_pos] == '\0') {
            // Done with this argument
            parser->optind++;
            parser->current_arg = NULL;
            parser->short_opt_pos = 0;
            return opt_parse_long(parser, argc, argv, optstring, longopts, longindex);
        }
        
        char c = arg[parser->short_opt_pos];
        int opt_type = find_short_option(optstring, c);
        
        if (opt_type >= 0) {
            parser->short_opt_pos++;
            
            // Check if we've finished this argument
            if (arg[parser->short_opt_pos] == '\0') {
                // Move to next argument
                parser->optind++;
                parser->current_arg = NULL;
                parser->short_opt_pos = 0;
            }
            
            if (opt_type == 1) { // required argument
                if (parser->short_opt_pos > 0 && arg[parser->short_opt_pos] != '\0') {
                    // Argument is in same string (e.g., "-fvalue")
                    parser->optarg = arg + parser->short_opt_pos;
                    parser->optind++;
                    parser->current_arg = NULL;
                    parser->short_opt_pos = 0;
                } else if (parser->optind < argc) {
                    // Argument is next argument (e.g., "-f value")
                    parser->optarg = argv[parser->optind];
                    parser->optind++;
                    parser->current_arg = NULL;
                    parser->short_opt_pos = 0;
                } else {
                    if (parser->opterr) {
                        fprintf(stderr, "%s: option requires an argument -- '%c'\n", argv[0], c);
                    }
                    parser->optopt = c;
                    return '?';
                }
            } else if (opt_type == 2) { // optional argument
                if (parser->short_opt_pos > 0 && arg[parser->short_opt_pos] != '\0') {
                    parser->optarg = arg + parser->short_opt_pos;
                    parser->optind++;
                    parser->current_arg = NULL;
                    parser->short_opt_pos = 0;
                }
            }
            
            return c;
        } else {
            // Unknown option
            if (parser->opterr) {
                fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], c);
            }
            parser->optopt = c;
            parser->short_opt_pos++;
            if (arg[parser->short_opt_pos] == '\0') {
                parser->optind++;
                parser->current_arg = NULL;
                parser->short_opt_pos = 0;
            }
            return '?';
        }
    }
    
    // Should not reach here
    parser->optind++;
    return -1;
}

int opt_parse(opt_parser_t* parser, int argc, char* const argv[], const char* optstring) {
    return opt_parse_long(parser, argc, argv, optstring, NULL, NULL);
}

