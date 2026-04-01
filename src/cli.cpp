#include "cli.hpp"

#include "core.hpp"
#include "config.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <getopt.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace ft {

static void print_help() {
  std::cout
    << "filetoggler [OPTIONS] [FILES...]\n\n"
    << "options:\n"
    << "    -C/--chdir DIR               Change workdir to the specified dir.\n"
    << "    -o/--open DIR                Open DIR instead of current directory.\n"
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
    std::cout << "filetoggler " << FILETOGGLER_VERSION << "\n";
}

bool parse_args(int argc, char** argv, ParsedArgs* out, std::string* err) {
    if (!out) {
        if (err) {
            *err = "internal: out is null";
        }
        return false;
    }

    ParsedArgs a;

    // First pass: handle -C/--chdir to set working directory before parsing other paths
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-C" || arg == "--chdir") {
            if (i + 1 >= argc) {
                if (err) {
                    *err = "missing value for --chdir";
                }
                return false;
            }
            a.cfg.chdir = argv[++i];
        }
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

    // Define long options for getopt_long
    static struct option long_options[] = {
        {"chdir",            required_argument, nullptr, 'C'},
        {"open",             required_argument, nullptr, 'o'},
        {"disabled-dir",     required_argument, nullptr, 'D'},
        {"disabled-prefix",  required_argument, nullptr, 'p'},
        {"disabled-suffix",  required_argument, nullptr, 's'},
        {"enable",           no_argument,       nullptr, 'e'},
        {"disable",          no_argument,       nullptr, 'd'},
        {"toggle",           no_argument,       nullptr, 't'},
        {"dry-run",          no_argument,       nullptr, 'n'},
        {"verbose",          no_argument,       nullptr, 'v'},
        {"quiet",            no_argument,       nullptr, 'q'},
        {"help",             no_argument,       nullptr, 'h'},
        {"version",          no_argument,       nullptr, 'V'},
        {"complete-bash",    required_argument, nullptr, 'c'},
        {nullptr, 0, nullptr, 0}
    };

    // Reset getopt state
    optind = 1;
    opterr = 0;  // Disable automatic error printing

    int c;
    while ((c = getopt_long(argc, argv, "C:o:D:p:s:edtnvqhVc:", long_options, nullptr)) != -1) {
        switch (c) {
            case 'C':
                // Already handled in first pass, skip argument
                if (optarg && optind < argc && argv[optind] && argv[optind][0] != '-') {
                    optind++;
                }
                break;

            case 'o':
                a.open_dir = optarg;
                break;

            case 'D':
                a.cfg.disabled_dir = optarg;
                break;

            case 'p':
                a.cfg.disabled_prefix = optarg;
                break;

            case 's':
                a.cfg.disabled_suffix = optarg;
                break;

            case 'e':
                a.action = Action::Enable;
                break;

            case 'd':
                a.action = Action::Disable;
                break;

            case 't':
                a.action = Action::Toggle;
                break;

            case 'n':
                a.cfg.dry_run = true;
                break;

            case 'v':
                a.cfg.verbosity = Verbosity::Verbose;
                break;

            case 'q':
                a.cfg.verbosity = Verbosity::Quiet;
                break;

            case 'h':
                a.show_help = true;
                break;

            case 'V':
                a.show_version = true;
                break;

            case 'c':
                a.mode = RunMode::Completion;
                a.completion_cword = std::atoi(optarg);
                for (int i = optind; i < argc; i++) {
                    a.completion_words.push_back(argv[i]);
                }
                break;

            case '?':
                if (err) {
                    *err = std::string("unknown option: ") + argv[optind - 1];
                }
                return false;

            case ':':
                if (err) {
                    *err = std::string("option requires an argument: ") + argv[optind - 1];
                }
                return false;

            default:
                if (err) {
                    *err = std::string("unexpected option: ") + argv[optind - 1];
                }
                return false;
        }
    }

    // Collect remaining non-option arguments as files
    for (int i = optind; i < argc; i++) {
        a.files.emplace_back(argv[i]);
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
        "-o", "--open",
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

        if (w == "-o" || w == "--open") {
            const std::string* v = next();
            if (v) {
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
    if (prev == "-C" || prev == "--chdir" || prev == "-D" || prev == "--disabled-dir" || prev == "-o" || prev == "--open") {
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
