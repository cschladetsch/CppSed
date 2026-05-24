// ============================================================
//  Regex.cpp  —  POSIX regex wrapper implementation
// ============================================================

#include "fastsed/Regex.hpp"
#include <cstring>

namespace fastsed {

static bool is_plain_literal(const string &pat, int flags) {
  if ((flags & REG_ICASE) != 0)
    return false;
  for (char c : pat) {
    switch (c) {
    case '\\':
    case '.':
    case '^':
    case '$':
    case '*':
    case '[':
    case ']':
    case '+':
    case '?':
    case '(':
    case ')':
    case '{':
    case '}':
    case '|':
      return false;
    default:
      break;
    }
  }
  return true;
}

RE::~RE() {
  if (ok)
    regfree(&r);
}

RE::RE(RE &&o) noexcept
    : r(o.r), ok(o.ok), src(std::move(o.src)), literal(o.literal),
      icase(o.icase), exact_empty(o.exact_empty) {
  o.ok = false;
  o.literal = false;
  o.icase = false;
  o.exact_empty = false;
}

RE &RE::operator=(RE &&o) noexcept {
  if (this != &o) {
    if (ok)
      regfree(&r);
    r = o.r;
    ok = o.ok;
    src = std::move(o.src);
    literal = o.literal;
    icase = o.icase;
    exact_empty = o.exact_empty;
    o.ok = false;
    o.literal = false;
    o.icase = false;
    o.exact_empty = false;
  }
  return *this;
}

void RE::compile(const string &pat, int flags) {
  if (ok) {
    regfree(&r);
    ok = false;
  }
  src = pat;
  icase = (flags & REG_ICASE) != 0;
  literal = is_plain_literal(pat, flags);
  exact_empty = !icase && pat == "^$";
  if (int e = regcomp(&r, pat.c_str(), flags); e) {
    char eb[512];
    regerror(e, &r, eb, sizeof eb);
    die(std::format("regex '{}': {}", pat, eb));
  }
  ok = true;
}

bool RE::exec(const char *s, size_t nm, regmatch_t *pm,
              int flags) const noexcept {
  if (exact_empty && (flags & REG_NOTBOL) == 0) {
    if (s[0] != '\0')
      return false;
    if (pm && nm > 0) {
      pm[0].rm_so = 0;
      pm[0].rm_eo = 0;
      for (size_t i = 1; i < nm; ++i) {
        pm[i].rm_so = -1;
        pm[i].rm_eo = -1;
      }
    }
    return true;
  }
  if (literal && !icase && (flags & REG_NOTBOL) == 0) {
    const char *pos = std::strstr(s, src.c_str());
    if (!pos)
      return false;
    if (pm && nm > 0) {
      pm[0].rm_so = static_cast<regoff_t>(pos - s);
      pm[0].rm_eo = pm[0].rm_so + static_cast<regoff_t>(src.size());
      for (size_t i = 1; i < nm; ++i) {
        pm[i].rm_so = -1;
        pm[i].rm_eo = -1;
      }
    }
    return true;
  }
  return ok && regexec(&r, s, nm, pm, flags) == 0;
}

bool RE::test(const char *s) const noexcept { return exec(s, 0, nullptr); }

} // namespace fastsed
