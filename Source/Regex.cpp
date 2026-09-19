// ============================================================
//  Regex.cpp  —  regex wrapper implementation (std::regex-backed)
// ============================================================

#include "fastsed/Regex.hpp"
#include <cstring>

namespace fastsed {

namespace {

bool is_plain_literal(const string &pat, int flags) {
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

// ── BRE/ERE -> ECMAScript translation ──────────────────────────
//
// std::regex's own `basic`/`extended` grammars implement strict
// POSIX BRE/ERE, which the C++ standard specifies WITHOUT pattern
// backreferences even for `extended` (glibc's ERE supports them as
// a common GNU extension; the standard's std::regex doesn't).
// `ECMAScript` is the one std::regex grammar that does support
// backreferences, and it already agrees with ERE on using bare
// ( ) { } + ? | as metacharacters — so both grammars are translated
// to ECMAScript rather than to std::regex's own basic/extended, to
// get backreference support uniformly. The translation's actual
// job is narrower than that sentence implies:
//   - ERE: only \< \> (GNU word-boundary anchors, not part of any
//     std::regex grammar) need mapping, to \b.
//   - BRE: \( \) \{ \} \+ \? \| are GNU BRE-extension escapes that
//     mean "treat as the ERE-style metacharacter"; their unescaped
//     counterparts are BRE literals that must be escaped for
//     ECMAScript, which treats them as metacharacters unescaped.
//     \< \> map to \b same as ERE. Backreferences (\1-\9) and
//     bracket expressions ([...], including [:class:]) already mean
//     the same thing in BRE and ECMAScript, so they pass through
//     unchanged — bracket contents are copied verbatim rather than
//     scanned for metacharacters, since ( ) + ? | are literal inside
//     a bracket expression in every one of these grammars anyway.
//
// Known residual gap: a literal backslash inside a bracket
// expression (no special meaning in POSIX) can be parsed differently
// by ECMAScript's own bracket-expression rules. Rare enough in
// practice (vs. \+ \? \| \< \> and ERE backreferences, which are
// common) that it's accepted rather than handled here.

// Copies a full bracket expression starting at p[i]=='[' into out
// verbatim, returning the index just past the closing ']'. Handles
// an optional leading '^', a literal ']' as the first member, and
// [:class:]/[.collating.]/[=equiv=] sub-constructs that contain ']'
// without ending the bracket expression.
size_t copy_bracket_expr(const string &p, size_t i, string &out) {
  out += p[i]; // '['
  ++i;
  if (i < p.size() && p[i] == '^') {
    out += p[i];
    ++i;
  }
  if (i < p.size() && p[i] == ']') {
    out += p[i]; // literal ']' as first member
    ++i;
  }
  while (i < p.size()) {
    if (p[i] == '[' && i + 1 < p.size() &&
        (p[i + 1] == ':' || p[i + 1] == '.' || p[i + 1] == '=')) {
      const char kind = p[i + 1];
      out += p[i];
      out += p[i + 1];
      i += 2;
      while (i + 1 < p.size() && !(p[i] == kind && p[i + 1] == ']')) {
        out += p[i];
        ++i;
      }
      if (i + 1 < p.size()) {
        out += p[i];
        out += p[i + 1];
        i += 2;
      }
      continue;
    }
    if (p[i] == ']') {
      out += p[i];
      return i + 1;
    }
    out += p[i];
    ++i;
  }
  return i; // unterminated — std::regex will raise regex_error
}

string bre_to_ecma(const string &p) {
  string out;
  out.reserve(p.size() + 8);
  size_t i = 0;
  while (i < p.size()) {
    const char c = p[i];
    if (c == '[') {
      i = copy_bracket_expr(p, i, out);
      continue;
    }
    if (c == '\\' && i + 1 < p.size()) {
      const char n = p[i + 1];
      switch (n) {
      case '(':
      case ')':
      case '{':
      case '}':
      case '+':
      case '?':
      case '|':
        out += n; // GNU BRE extension escape -> ECMAScript metachar
        break;
      case '<':
      case '>':
        out += "\\b"; // GNU word-boundary anchor -> closest ECMAScript equivalent
        break;
      default:
        out += c;
        out += n; // \1-\9, \., \*, \\, etc. — same meaning in both
        break;
      }
      i += 2;
      continue;
    }
    if (c == '(' || c == ')' || c == '{' || c == '}' || c == '+' ||
        c == '?' || c == '|') {
      out += '\\';
      out += c; // literal in BRE -> must be escaped for ECMAScript
      ++i;
      continue;
    }
    out += c;
    ++i;
  }
  return out;
}

string ere_to_ecma(const string &p) {
  string out;
  out.reserve(p.size());
  size_t i = 0;
  while (i < p.size()) {
    const char c = p[i];
    if (c == '[') {
      i = copy_bracket_expr(p, i, out);
      continue;
    }
    if (c == '\\' && i + 1 < p.size()) {
      const char n = p[i + 1];
      if (n == '<' || n == '>') {
        out += "\\b";
      } else {
        out += c;
        out += n;
      }
      i += 2;
      continue;
    }
    out += c;
    ++i;
  }
  return out;
}

} // namespace

void RE::compile(const string &pat, int flags) {
  ok = false;
  src = pat;
  icase = (flags & REG_ICASE) != 0;
  literal = is_plain_literal(pat, flags);
  exact_empty = !icase && pat == "^$";

  const bool extended = (flags & REG_EXTENDED) != 0;
  const string ecma_pat = extended ? ere_to_ecma(pat) : bre_to_ecma(pat);

  auto opts = std::regex_constants::ECMAScript;
  if (icase)
    opts |= std::regex_constants::icase;

  try {
    re = std::regex(ecma_pat, opts);
  } catch (const std::regex_error &e) {
    die(std::format("regex '{}': {}", pat, e.what()));
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

  if (!ok)
    return false;

  auto search_flags = std::regex_constants::match_default;
  if (flags & REG_NOTBOL)
    search_flags |= std::regex_constants::match_not_bol;

  std::cmatch m;
  // regex_search's noexcept-ness isn't guaranteed by the standard,
  // but a bad_alloc/regex_error here would already have surfaced at
  // compile() time for any pattern this engine accepts; treat a
  // stray exception as "no match" rather than letting it propagate
  // through a noexcept function (which would call std::terminate).
  bool matched;
  try {
    matched = std::regex_search(s, m, re, search_flags);
  } catch (...) {
    return false;
  }
  if (!matched)
    return false;

  if (pm) {
    const size_t have = m.size(); // whole match + submatches
    for (size_t i = 0; i < nm; ++i) {
      if (i < have && m[i].matched) {
        pm[i].rm_so = static_cast<regoff_t>(m.position(i));
        pm[i].rm_eo = pm[i].rm_so + static_cast<regoff_t>(m.length(i));
      } else {
        pm[i].rm_so = -1;
        pm[i].rm_eo = -1;
      }
    }
  }
  return true;
}

bool RE::test(const char *s) const noexcept { return exec(s, 0, nullptr); }

} // namespace fastsed
