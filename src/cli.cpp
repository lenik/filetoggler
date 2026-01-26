#include "cli.hpp"

#include "core.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

namespace ft {

static void print_help() {
  std::cout
    << "filetoggler [OPTIONS] [FILES...]\n\n"
    << "options:\n"
    << "    -C/--chdir DIR               Change workdir to the specified dir.\n"
    << "    -D/--disabled-dir DIR        Disabled file will goes to this directory.\n"
    << "    -p/--disabled-prefix PREFIX  Disabled file will add this prefix before filename\n"
    << "    -s/--disabled-suffix SUFFIX  Disabled file will add this suffix after filename\n"
    << "    -e/--enable                  Enable the specified files\n"
    << "    -d/--disable                 Disable the specified files\n"
    << "    -t/--toggle                  Toggle between enabled/disabled for the specified files\n"
    << "    -n/--dry-run\n"
    << "    -v/--verbose\n"
    << "    -q/--quiet\n"
    << "    -h/--help\n"
    << "    --version\n";
}

static void print_version() {
    std::cout << "filetoggler 0.1.0\n";
}

static bool is_option(std::string_view s) {
    return !s.empty() && s[0] == '-';
}

bool parse_args(int argc, char** argv, ParsedArgs* out, std::string* err) {
    if (!out) {
        if (err) {
            *err = "internal: out is null";
        }
        return false;
    }

    ParsedArgs a;

    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            a.show_help = true;
            continue;
        }
        if (arg == "--version") {
            a.show_version = true;
            continue;
        }

        if (arg == "-C" || arg == "--chdir") {
            if (i + 1 >= argc) {
                if (err) {
                    *err = "missing value for --chdir";
                }
                return false;
            }
            a.cfg.chdir = argv[++i];
            continue;
        }

        if (arg == "-D" || arg == "--disabled-dir") {
            if (i + 1 >= argc) {
                if (err) {
                    *err = "missing value for --disabled-dir";
                }
                return false;
            }
            a.cfg.disabled_dir = argv[++i];
            continue;
        }

        if (arg == "-p" || arg == "--disabled-prefix") {
            if (i + 1 >= argc) {
                if (err) {
                    *err = "missing value for --disabled-prefix";
                }
                return false;
            }
            a.cfg.disabled_prefix = argv[++i];
            continue;
        }

        if (arg == "-s" || arg == "--disabled-suffix") {
            if (i + 1 >= argc) {
                if (err) {
                    *err = "missing value for --disabled-suffix";
                }
                return false;
            }
            a.cfg.disabled_suffix = argv[++i];
            continue;
        }

        if (arg == "-e" || arg == "--enable") {
            a.action = Action::Enable;
            continue;
        }
        if (arg == "-d" || arg == "--disable") {
            a.action = Action::Disable;
            continue;
        }
        if (arg == "-t" || arg == "--toggle") {
            a.action = Action::Toggle;
            continue;
        }

        if (arg == "-n" || arg == "--dry-run") {
            a.cfg.dry_run = true;
            continue;
        }

        if (arg == "-v" || arg == "--verbose") {
            a.cfg.verbosity = Verbosity::Verbose;
            continue;
        }

        if (arg == "-q" || arg == "--quiet") {
            a.cfg.verbosity = Verbosity::Quiet;
            continue;
        }

        if (arg == "--complete-bash") {
            a.mode = RunMode::Completion;
            if (i + 1 >= argc) {
                if (err) {
                    *err = "missing cword for --complete-bash";
                }
                return false;
            }
            a.completion_cword = std::atoi(argv[++i]);
            for (int j = i + 1; j < argc; j++) {
                a.completion_words.push_back(argv[j]);
            }
            break;
        }

        if (is_option(arg)) {
            if (err) {
                *err = std::string("unknown option: ") + std::string(arg);
            }
            return false;
        }

        a.files.emplace_back(arg);
    }

    if (!a.cfg.chdir.empty()) {
        std::error_code ec;
        fs::current_path(a.cfg.chdir, ec);
        if (ec) {
            if (err) {
                *err = std::string("chdir failed: ") + a.cfg.chdir.string() + ": " + ec.message();
            }
            return false;
        }
    }

    if (a.mode != RunMode::Completion) {
        const bool has_tty = (::isatty(STDIN_FILENO) == 1) && (::isatty(STDOUT_FILENO) == 1);

        if (a.show_help || a.show_version) {
            a.mode = RunMode::Cli;
        } else if (!has_tty) {
            a.mode = RunMode::Gui;
        } else if (a.files.empty()) {
            a.mode = RunMode::Gui;
        } else {
            a.mode = RunMode::Cli;
        }
    }

    *out = std::move(a);
    return true;
}

static int apply_action_to_files(Action act, const std::vector<std::string>& files, const Config& cfg) {
    int rc = 0;
    for (const auto& f : files) {
        fs::path p = f;
        std::string e;
        bool ok = false;
        switch (act) {
            case Action::Enable:
                ok = enable_one(p, cfg, &e);
                break;
            case Action::Disable:
                ok = disable_one(p, cfg, &e);
                break;
            case Action::Toggle:
                ok = toggle_one(p, cfg, &e);
                break;
            case Action::None:
                ok = true;
                break;
        }

        if (!ok) {
            if (cfg.verbosity != Verbosity::Quiet) {
                std::cerr << e << "\n";
            }
            rc = 2;
        }
    }
    return rc;
}

int run_cli(const ParsedArgs& args) {
    if (args.show_help) {
        print_help();
        return 0;
    }
    if (args.show_version) {
        print_version();
        return 0;
    }

    Action act = args.action;
    if (act == Action::None) {
        act = Action::Toggle;
    }

    if (args.files.empty()) {
        if (args.cfg.verbosity != Verbosity::Quiet) {
            std::cerr << "no files specified\n";
        }
        return 2;
    }

    return apply_action_to_files(act, args.files, args.cfg);
}

static std::vector<std::string> complete_options(std::string_view prefix) {
    static const std::vector<std::string> opts = {
        "-C", "--chdir",
        "-D", "--disabled-dir",
        "-p", "--disabled-prefix",
        "-s", "--disabled-suffix",
        "-e", "--enable",
        "-d", "--disable",
        "-t", "--toggle",
        "-n", "--dry-run",
        "-v", "--verbose",
        "-q", "--quiet",
        "-h", "--help",
        "--version",
    };

    std::vector<std::string> out;
    for (const auto& o : opts) {
        if (o.starts_with(prefix)) {
            out.push_back(o);
        }
    }
    return out;
}

static std::vector<std::string> complete_dirs(std::string_view prefix) {
    std::vector<std::string> out;

    fs::path base_dir = fs::current_path();
    fs::path typed_path = std::string(prefix);
    fs::path parent = typed_path.parent_path();
    fs::path leaf = typed_path.filename();

    fs::path scan_dir = parent.empty() ? base_dir : (base_dir / parent);

    std::error_code ec;
    for (const auto& de : fs::directory_iterator(scan_dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            break;
        }

        if (!de.is_directory(ec)) {
            continue;
        }

        const std::string name = de.path().filename().string();
        if (!leaf.empty() && !std::string_view(name).starts_with(leaf.string())) {
            continue;
        }

        fs::path candidate = parent.empty() ? fs::path(name) : (parent / name);
        out.push_back(candidate.string());
    }

    return out;
}

static void parse_completion_config(const ParsedArgs& args, Config* cfg, fs::path* completion_base_dir) {
    if (!cfg || !completion_base_dir) {
        return;
    }

    *completion_base_dir = fs::current_path();

    for (size_t i = 1; i < args.completion_words.size(); i++) {
        const std::string& w = args.completion_words[i];
        const auto next = [&]() -> const std::string* {
            if (i + 1 >= args.completion_words.size()) {
                return nullptr;
            }
            return &args.completion_words[i + 1];
        };

        if (w == "-C" || w == "--chdir") {
            const std::string* v = next();
            if (v) {
                *completion_base_dir = *v;
                i++;
            }
            continue;
        }

        if (w == "-D" || w == "--disabled-dir") {
            const std::string* v = next();
            if (v) {
                cfg->disabled_dir = *v;
                i++;
            }
            continue;
        }

        if (w == "-p" || w == "--disabled-prefix") {
            const std::string* v = next();
            if (v) {
                cfg->disabled_prefix = *v;
                i++;
            }
            continue;
        }

        if (w == "-s" || w == "--disabled-suffix") {
            const std::string* v = next();
            if (v) {
                cfg->disabled_suffix = *v;
                i++;
            }
            continue;
        }
    }
}

static std::vector<std::string> complete_files(std::string_view prefix, const Config& cfg) {
    std::vector<std::string> out;

    std::string pfx(prefix);
    fs::path base_dir = fs::current_path();
    fs::path typed_path = pfx;
    fs::path parent = typed_path.parent_path();
    fs::path leaf = typed_path.filename();

    fs::path scan_dir = parent.empty() ? base_dir : (base_dir / parent);

    std::error_code ec;
    auto entries = list_dir_entries_with_disabled(scan_dir, cfg);
    for (const auto& e : entries) {
        if (!leaf.empty()) {
            if (!std::string_view(e.display_name).starts_with(leaf.string())) {
                continue;
            }
        }

        fs::path candidate = parent.empty() ? fs::path(e.display_name) : (parent / e.display_name);
        out.push_back(candidate.string());
    }

    return out;
}

int run_completion(const ParsedArgs& args) {
    int cword = args.completion_cword;
    if (cword < 0) {
        return 0;
    }

    if (args.completion_words.empty()) {
        return 0;
    }

    std::string current;
    if (cword >= 0 && cword < static_cast<int>(args.completion_words.size())) {
        current = args.completion_words[cword];
    }

    std::string prev;
    if (cword - 1 >= 0 && cword - 1 < static_cast<int>(args.completion_words.size())) {
        prev = args.completion_words[cword - 1];
    }

    Config cfg = args.cfg;
    fs::path completion_base_dir;
    parse_completion_config(args, &cfg, &completion_base_dir);
    std::error_code ec;
    fs::current_path(completion_base_dir, ec);

    std::vector<std::string> results;
    if (prev == "-C" || prev == "--chdir" || prev == "-D" || prev == "--disabled-dir") {
        results = complete_dirs(current);
    } else if (!current.empty() && current[0] == '-') {
        results = complete_options(current);
    } else {
        results = complete_files(current, cfg);
    }

    for (const auto& r : results) {
        std::cout << r << "\n";
    }
    return 0;
}

}
