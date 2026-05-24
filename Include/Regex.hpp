#pragma once
// ============================================================
//  Regex.hpp  —  compile-once POSIX regex wrapper
//  Uses regcomp/regexec directly; measurably faster than
//  std::regex or Boost.Regex for the short patterns sed uses.
// ============================================================

#include "fastsed/Common.hpp"
#include <regex.h>

namespace fastsed {

struct RE {
  regex_t r{};
  bool ok = false;
  string src; // original pattern (for error messages)

  RE() = default;
  ~RE();
  RE(const RE &) = delete;
  RE &operator=(const RE &) = delete;
  RE(RE &&) noexcept;
  RE &operator=(RE &&) noexcept;

  // Compile pat with POSIX flags (REG_EXTENDED, REG_ICASE, …).
  // Calls die() on any regex error.
  void compile(const string &pat, int flags);

  // Returns true on match. nm/pm are the sub-match arrays.
  bool exec(const char *s, size_t nm, regmatch_t *pm,
            int flags = 0) const noexcept;

  bool test(const char *s) const noexcept;
};

} // namespace fastsed
