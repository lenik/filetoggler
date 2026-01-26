#include "../src/core.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

static fs::path makeTempDir() {
    fs::path base = fs::temp_directory_path();
    for (int i = 0; i < 1000; i++) {
        fs::path p = base / ("filetoggler_test_" + std::to_string(::getpid()) + "_" + std::to_string(i));
        std::error_code ec;
        if (fs::create_directory(p, ec)) {
            return p;
        }
    }
    throw std::runtime_error("failed to create temp dir");
}

static void writeFile(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p);
    f << content;
}

static bool existsRegular(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec) && fs::is_regular_file(p, ec);
}

static void testDecorateUndecorate() {
    ft::Config cfg;
    cfg.disabled_prefix = ".";
    cfg.disabled_suffix = ".off";

    const std::string original = "demo.txt";
    const std::string decorated = ft::decorate_disabled_name(original, cfg);
    assert(decorated == ".demo.txt.off");

    const auto back = ft::undecorate_disabled_name(decorated, cfg);
    assert(back.has_value());
    assert(*back == original);

    const auto no = ft::undecorate_disabled_name("xxx", cfg);
    assert(!no.has_value());
}

static void testDisableEnableRoundtrip() {
    fs::path dir = makeTempDir();

    ft::Config cfg;
    cfg.disabled_dir = ".disabled.d";

    fs::path enabled = dir / "a.txt";
    writeFile(enabled, "hello");

    std::string err;
    assert(ft::get_state(enabled, cfg) == ft::FileState::Enabled);

    assert(ft::disable_one(enabled, cfg, &err));
    assert(!existsRegular(enabled));
    assert(existsRegular(dir / cfg.disabled_dir / "a.txt"));

    assert(ft::get_state(enabled, cfg) == ft::FileState::Disabled);

    assert(ft::enable_one(enabled, cfg, &err));
    assert(existsRegular(enabled));
    assert(!existsRegular(dir / cfg.disabled_dir / "a.txt"));

    fs::remove_all(dir);
}

static void testDisableWithPrefixSuffix() {
    fs::path dir = makeTempDir();

    ft::Config cfg;
    cfg.disabled_dir = ".disabled.d";
    cfg.disabled_prefix = "__";
    cfg.disabled_suffix = "~";

    fs::path enabled = dir / "b.txt";
    writeFile(enabled, "hello");

    std::string err;
    assert(ft::disable_one(enabled, cfg, &err));
    assert(!existsRegular(enabled));
    assert(existsRegular(dir / cfg.disabled_dir / "__b.txt~"));

    assert(ft::enable_one(enabled, cfg, &err));
    assert(existsRegular(enabled));

    fs::remove_all(dir);
}

static void testListDirShowsOriginalNames() {
    fs::path dir = makeTempDir();

    ft::Config cfg;
    cfg.disabled_dir = ".disabled.d";
    cfg.disabled_prefix = "__";
    cfg.disabled_suffix = "~";

    writeFile(dir / "x.txt", "x");
    writeFile(dir / cfg.disabled_dir / "__y.txt~", "y");

    auto entries = ft::list_dir_entries_with_disabled(dir, cfg);

    bool sawX = false;
    bool sawY = false;
    for (const auto& e : entries) {
        if (e.display_name == "x.txt") {
            sawX = true;
            assert(e.state == ft::FileState::Enabled);
        }
        if (e.display_name == "y.txt") {
            sawY = true;
            assert(e.state == ft::FileState::Disabled);
            assert(e.enabled_path == dir / "y.txt");
            assert(e.disabled_path == dir / cfg.disabled_dir / "__y.txt~");
        }
    }

    assert(sawX);
    assert(sawY);

    fs::remove_all(dir);
}

int main() {
    try {
        testDecorateUndecorate();
        testDisableEnableRoundtrip();
        testDisableWithPrefixSuffix();
        testListDirShowsOriginalNames();
    } catch (const std::exception& e) {
        std::cerr << "test failure: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
