#pragma once
// ============================================================
//  Regex.hpp  —  compile-once regex wrapper over std::regex
//
//  Previously wrapped POSIX regcomp/regexec directly; <regex.h>
//  has no MSVC implementation at all (not just a missing header —
//  the functions don't exist in the MSVC CRT), so this now sits on
//  std::regex instead, which is portable everywhere including
//  MSVC. std::regex_constants::basic / extended approximate POSIX
//  BRE / ERE; exact GNU-sed-specific BRE escapes (\+, \?, \|, word
//  boundaries, etc.) may differ slightly from glibc's regcomp —
//  see the integration test suite for what's covered.
//
//  The public surface (RE, regmatch_t, REG_EXTENDED, REG_ICASE,
//  REG_NOTBOL) is kept POSIX-shaped on purpose so Parser.cpp,
//  EngineHelpers.cpp, and Replacement.cpp/.hpp needed no changes
//  beyond this header and Regex.cpp.
// ============================================================

#include "Common.hpp"
#include <regex>

namespace fastsed {

// ── POSIX-shaped compatibility types ────────────────────────────
using regoff_t = long long;

struct regmatch_t {
  regoff_t rm_so = -1;
  regoff_t rm_eo = -1;
};

// Compile-time flags (compile()'s `flags` parameter). Bit values are
// ours alone now — nothing calls the real regcomp, so there's no
// need to match glibc's REG_* numeric values.
inline constexpr int REG_EXTENDED = 1 << 0;
inline constexpr int REG_ICASE = 1 << 1;

// Execution-time flag (exec()'s `flags` parameter).
inline constexpr int REG_NOTBOL = 1 << 0;

struct RE {
  std::regex re;
  bool ok = false;
  string src; // original pattern (for error messages)
  bool literal = false;
  bool icase = false;
  bool exact_empty = false;

  RE() = default;
  ~RE() = default;
  RE(const RE &) = delete;
  RE &operator=(const RE &) = delete;
  RE(RE &&) noexcept = default;
  RE &operator=(RE &&) noexcept = default;

  // Compile pat with REG_EXTENDED / REG_ICASE. Calls die() on any
  // regex error (std::regex_error).
  void compile(const string &pat, int flags);

  // Returns true on match. nm/pm are the sub-match arrays, same
  // rm_so/rm_eo-per-slot convention as POSIX regexec(); unmatched
  // groups (or groups beyond what the pattern has) get (-1, -1).
  bool exec(const char *s, size_t nm, regmatch_t *pm,
            int flags = 0) const noexcept;

  bool test(const char *s) const noexcept;

  bool is_literal() const noexcept { return literal; }
};

} // namespace fastsed
