#include "cli.hpp"
#include "gui.hpp"

#include "proc/dbgthread.h"
#include "proc/stackdump.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const stackdump_color_schema_t *color_schema = NULL;
    g_interactive = isatty(0);
    if (g_interactive) {
        color_schema = &stackdump_color_schema_default;
    }
    stackdump_install_crash_handler(color_schema);

    ft::ParsedArgs args;
    std::string err;
    if (!ft::parse_args(argc, argv, &args, &err)) {
        std::cerr << err << "\n";
        return 2;
    }

    if (args.mode == ft::RunMode::Completion) {
        return ft::run_completion(args);
    }

    if (args.mode == ft::RunMode::Cli) {
        return ft::run_cli(args);
    }

    void* ctx = start_dbg_thread();

    int status = ft::run_gui(args.cfg, args.files);

    stop_dbg_thread(ctx);
    
    return status;
}
