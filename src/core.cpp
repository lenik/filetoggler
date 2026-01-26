#include "core.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <unordered_map>

namespace fs = std::filesystem;

namespace ft {

static void log_line(const Config& cfg, std::string_view msg) {
    if (cfg.verbosity == Verbosity::Quiet) {
        return;
    }
    std::cerr << msg << "\n";
}

std::string decorate_disabled_name(std::string_view original, const Config& cfg) {
    std::string out;
    out.reserve(cfg.disabled_prefix.size() + original.size() + cfg.disabled_suffix.size());
    out.append(cfg.disabled_prefix);
    out.append(original);
    out.append(cfg.disabled_suffix);
    return out;
}

std::optional<std::string> undecorate_disabled_name(std::string_view decorated, const Config& cfg) {
    if (decorated.size() < cfg.disabled_prefix.size() + cfg.disabled_suffix.size()) {
        return std::nullopt;
    }

    if (!cfg.disabled_prefix.empty()) {
        if (!decorated.starts_with(cfg.disabled_prefix)) {
            return std::nullopt;
        }
    }

    if (!cfg.disabled_suffix.empty()) {
        if (!decorated.ends_with(cfg.disabled_suffix)) {
            return std::nullopt;
        }
    }

    const size_t start = cfg.disabled_prefix.size();
    const size_t end = decorated.size() - cfg.disabled_suffix.size();
    if (end < start) {
        return std::nullopt;
    }

    return std::string(decorated.substr(start, end - start));
}

fs::path disabled_path_for(const fs::path& enabled_path, const Config& cfg) {
    fs::path base = enabled_path.parent_path();
    fs::path dd = base / cfg.disabled_dir;
    std::string name = enabled_path.filename().string();
    return dd / decorate_disabled_name(name, cfg);
}

FileState get_state(const fs::path& enabled_path, const Config& cfg) {
    std::error_code ec;
    if (fs::exists(enabled_path, ec)) {
        return FileState::Enabled;
    }

    fs::path dp = disabled_path_for(enabled_path, cfg);
    if (fs::exists(dp, ec)) {
        return FileState::Disabled;
    }

    return FileState::Missing;
}

void ensure_disabled_dir_exists(const fs::path& base_dir, const Config& cfg, bool dry_run) {
    fs::path dd = base_dir / cfg.disabled_dir;
    std::error_code ec;
    if (fs::exists(dd, ec)) {
        return;
    }
    if (dry_run) {
        return;
    }
    fs::create_directories(dd, ec);
}

void move_path(const fs::path& from, const fs::path& to, const Config& cfg) {
    if (cfg.verbosity == Verbosity::Verbose) {
        log_line(cfg, std::string("move: ") + from.string() + " -> " + to.string());
    }

    if (cfg.dry_run) {
        return;
    }

    std::error_code ec;
    fs::rename(from, to, ec);
    if (!ec) {
        return;
    }

    if (ec == std::errc::cross_device_link) {
        fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            throw fs::filesystem_error("copy_file", from, to, ec);
        }
        fs::remove(from, ec);
        if (ec) {
            throw fs::filesystem_error("remove", from, ec);
        }
        return;
    }

    throw fs::filesystem_error("rename", from, to, ec);
}

bool enable_one(const fs::path& enabled_path, const Config& cfg, std::string* err) {
    try {
        fs::path dp = disabled_path_for(enabled_path, cfg);
        std::error_code ec;
        if (!fs::exists(dp, ec)) {
            if (err) {
                *err = "disabled file not found: " + dp.string();
            }
            return false;
        }

        move_path(dp, enabled_path, cfg);
        return true;
    } catch (const std::exception& e) {
        if (err) {
            *err = e.what();
        }
        return false;
    }
}

bool disable_one(const fs::path& enabled_path, const Config& cfg, std::string* err) {
    try {
        std::error_code ec;
        if (!fs::exists(enabled_path, ec)) {
            if (err) {
                *err = "enabled file not found: " + enabled_path.string();
            }
            return false;
        }

        ensure_disabled_dir_exists(enabled_path.parent_path(), cfg, cfg.dry_run);

        fs::path dp = disabled_path_for(enabled_path, cfg);
        move_path(enabled_path, dp, cfg);
        return true;
    } catch (const std::exception& e) {
        if (err) {
            *err = e.what();
        }
        return false;
    }
}

bool toggle_one(const fs::path& enabled_path, const Config& cfg, std::string* err) {
    switch (get_state(enabled_path, cfg)) {
        case FileState::Enabled:
            return disable_one(enabled_path, cfg, err);
        case FileState::Disabled:
            return enable_one(enabled_path, cfg, err);
        case FileState::Missing:
            break;
    }
    if (err) {
        *err = "file not found (enabled or disabled): " + enabled_path.string();
    }
    return false;
}

static std::optional<FileEntry> build_enabled_entry(const fs::path& p, const Config& cfg) {
    std::error_code ec;
    fs::file_status st = fs::status(p, ec);
    if (ec) {
        return std::nullopt;
    }

    FileEntry e;
    e.display_name = p.filename().string();
    e.enabled_path = p;
    e.disabled_path = disabled_path_for(p, cfg);
    e.is_dir = fs::is_directory(st);
    e.state = FileState::Enabled;
    if (!e.is_dir) {
        e.size = fs::file_size(p, ec);
        if (ec) {
            e.size = 0;
        }
    }
    e.mtime = fs::last_write_time(p, ec);
    return e;
}

static std::optional<FileEntry> build_disabled_entry(const fs::path& disabled_file, const std::string& original_name, const fs::path& dir, const Config&) {
    std::error_code ec;
    fs::file_status st = fs::status(disabled_file, ec);
    if (ec) {
        return std::nullopt;
    }

    FileEntry e;
    e.display_name = original_name;
    e.enabled_path = dir / original_name;
    e.disabled_path = disabled_file;
    e.is_dir = fs::is_directory(st);
    e.state = FileState::Disabled;
    if (!e.is_dir) {
        e.size = fs::file_size(disabled_file, ec);
        if (ec) {
            e.size = 0;
        }
    }
    e.mtime = fs::last_write_time(disabled_file, ec);
    return e;
}

std::vector<FileEntry> list_dir_entries_with_disabled(const fs::path& dir, const Config& cfg) {
    std::vector<FileEntry> out;

    std::unordered_map<std::string, size_t> by_name;

    std::error_code ec;
    for (const auto& de : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            break;
        }

        fs::path p = de.path();
        if (p.filename() == cfg.disabled_dir) {
            continue;
        }

        auto eopt = build_enabled_entry(p, cfg);
        if (!eopt) {
            continue;
        }

        by_name[eopt->display_name] = out.size();
        out.push_back(std::move(*eopt));
    }

    fs::path dd = dir / cfg.disabled_dir;
    if (fs::exists(dd, ec) && fs::is_directory(dd, ec)) {
        for (const auto& de : fs::directory_iterator(dd, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) {
                break;
            }

            fs::path p = de.path();
            std::string decorated = p.filename().string();
            auto original_opt = undecorate_disabled_name(decorated, cfg);
            if (!original_opt) {
                continue;
            }

            auto it = by_name.find(*original_opt);
            if (it != by_name.end()) {
                FileEntry& existing = out[it->second];
                existing.state = FileState::Disabled;
                existing.disabled_path = p;
                existing.mtime = fs::last_write_time(p, ec);
                if (!existing.is_dir) {
                    existing.size = fs::file_size(p, ec);
                    if (ec) {
                        existing.size = 0;
                    }
                }
                continue;
            }

            auto eopt2 = build_disabled_entry(p, *original_opt, dir, cfg);
            if (!eopt2) {
                continue;
            }
            by_name[eopt2->display_name] = out.size();
            out.push_back(std::move(*eopt2));
        }
    }

    std::sort(out.begin(), out.end(), [](const FileEntry& a, const FileEntry& b) {
        return a.display_name < b.display_name;
    });

    return out;
}

}
