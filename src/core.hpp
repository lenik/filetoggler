#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ft {

enum class Verbosity {
    Quiet,
    Normal,
    Verbose,
};

struct Config {
    std::filesystem::path chdir;
    std::filesystem::path disabled_dir{ ".disable.d" };
    std::string disabled_prefix;
    std::string disabled_suffix;
    bool dry_run{false};
    Verbosity verbosity{Verbosity::Normal};
};

enum class FileState {
    Enabled,
    Disabled,
    Missing,
};

struct FileEntry {
    std::string display_name;
    std::filesystem::path enabled_path;
    std::filesystem::path disabled_path;
    std::filesystem::file_time_type mtime{};
    std::uintmax_t size{0};
    bool is_dir{false};
    FileState state{FileState::Missing};
};

std::string decorate_disabled_name(std::string_view original, const Config& cfg);
std::optional<std::string> undecorate_disabled_name(std::string_view decorated, const Config& cfg);

std::filesystem::path disabled_path_for(const std::filesystem::path& enabled_path, const Config& cfg);

FileState get_state(const std::filesystem::path& enabled_path, const Config& cfg);

void ensure_disabled_dir_exists(const std::filesystem::path& base_dir, const Config& cfg, bool dry_run);

void move_path(const std::filesystem::path& from, const std::filesystem::path& to, const Config& cfg);

bool enable_one(const std::filesystem::path& enabled_path, const Config& cfg, std::string* err);
bool disable_one(const std::filesystem::path& enabled_path, const Config& cfg, std::string* err);
bool toggle_one(const std::filesystem::path& enabled_path, const Config& cfg, std::string* err);
bool rename_one(const std::filesystem::path& enabled_path, std::string_view new_display_name, const Config& cfg, std::string* err);

std::vector<FileEntry> list_dir_entries_with_disabled(const std::filesystem::path& dir, const Config& cfg);

}
