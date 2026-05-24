#pragma once
// ============================================================
//  Common.hpp  —  shared includes, type aliases, globals
// ============================================================

#include "fastsed/Print.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unistd.h>
#include <variant>
#include <vector>

namespace fastsed {

namespace fs = std::filesystem;
using std::string, std::string_view, std::vector, std::shared_ptr,
    std::make_shared, std::optional;

// ── Global state ─────────────────────────────────────────────
// Defined in Common.cpp; extern everywhere else.
extern int g_exit_code; // set by q / Q commands

// ── Diagnostics ──────────────────────────────────────────────
[[noreturn]] void die(string_view msg);
void warn(string_view msg);

} // namespace fastsed
