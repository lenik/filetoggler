#pragma once

#include "core.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace ft {

enum class RunMode {
    Gui,
    Cli,
    Completion,
};

enum class Action {
    None,
    Enable,
    Disable,
    Toggle,
};

struct ParsedArgs {
    RunMode mode{RunMode::Gui};
    Action action{Action::None};
    Config cfg;
    std::vector<std::string> files;

    bool show_help{false};
    bool show_version{false};

    bool completion_mode{false};
    int completion_cword{-1};
    std::vector<std::string> completion_words;

    std::optional<fs::path> open_dir;
};

bool parse_args(int argc, char** argv, ParsedArgs* out, std::string* err);

int run_cli(const ParsedArgs& args);

int run_completion(const ParsedArgs& args);

}
