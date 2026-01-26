#include "cli.hpp"
#include "gui.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
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

    return ft::run_gui(args.cfg, args.files);
}
