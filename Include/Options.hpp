#pragma once
// ============================================================
//  Options.hpp  —  CLI option bag + parse_args()
//  Parsing is done by Boost.Program_options (Options.cpp).
// ============================================================

#include "fastsed/Common.hpp"

namespace fastsed {

struct Options {
  vector<string> scripts;    // -e / -f content, concatenated
  vector<string> files;      // remaining positional arguments
  string inplace_suf;        // -i[suffix]  ("" → no in-place)
  bool suppress = false;     // -n
  bool extended_re = false;  // -E / -r
  bool separate = false;     // -s
  bool null_delim = false;   // -z
  bool sandbox = false;      // --sandbox
  bool first_hash_n = false; // script starts with #n
};

// Parses argc/argv.  Populates opts.scripts with concatenated
// script text ready to hand to Parser.  Calls die() on errors.
Options parse_args(int argc, char **argv);

} // namespace fastsed
