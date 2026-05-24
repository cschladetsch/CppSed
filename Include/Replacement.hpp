#pragma once
// ============================================================
//  Replacement.hpp  —  s/// replacement pre-compilation
//  The replacement string is parsed once into a RTok list;
//  apply_repl() walks it on every match — no repeated parsing.
// ============================================================

#include "fastsed/Common.hpp"
#include <regex.h>

namespace fastsed {

// Token kinds inside a replacement string
enum class RTK {
  Lit,     // literal characters
  All,     // & — entire match
  Ref,     // \1 … \9 — back-reference
  NL,      // \n — newline literal
  LowerC,  // \l — lowercase next character
  UpperC,  // \u — uppercase next character
  LowerOn, // \L — lowercase until \E
  UpperOn, // \U — uppercase until \E
  CaseOff, // \E — end case conversion
};

struct RTok {
  RTK k;
  string lit;  // payload for Lit
  int ref = 0; // payload for Ref (1-9)
};

using ReplVec = vector<RTok>;

// Parse a raw replacement string (after delimiter stripping) into tokens.
ReplVec parse_repl(string_view sv);

// Apply a parsed replacement against a successful regexec() result.
// src  : pointer to the start of the string that was searched
// pm   : regmatch_t array from regexec()
// npm  : size of pm array
string apply_repl(const ReplVec &rv, const char *src, const regmatch_t *pm,
                  int npm);

} // namespace fastsed
