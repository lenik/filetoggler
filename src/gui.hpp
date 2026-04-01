#pragma once

#include "core.hpp"

#include <wx/wx.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace ft {

int run_gui(const Config& cfg, const std::vector<std::string>& files, const std::optional<fs::path>& open_dir = std::nullopt);

}
