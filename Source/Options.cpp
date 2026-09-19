// ============================================================
//  Options.cpp  —  CLI argument parsing (hand-rolled, GNU-getopt
//  style; replaces Boost.Program_options)
// ============================================================

#include "fastsed/Options.hpp"
#include "fastsed/Parser.hpp"
#include "fastsed/Print.hpp"

#include <fstream>
#include <sstream>

namespace fastsed {

namespace {

// Sentinel stored in Options::inplace_suf for a bare "-i"/"--inplace"
// (in-place edit, no backup). Main.cpp maps it back to an empty
// suffix; distinct from "" itself, which means -i wasn't given at
// all (see Options.hpp).
constexpr char kBareInplace = '\x01';

void print_usage() {
  std::println(stdout, "fastsed (built {} {})", __DATE__, __TIME__);
  std::println(stdout, "Usage: sed [OPTION]... SCRIPT [FILE]...");
  std::println(stdout, "       sed [OPTION]... -e SCRIPT... [FILE]...");
  std::println(stdout, "       sed [OPTION]... -f SCRIPTFILE... [FILE]...");
  std::println(stdout, "Options:");
  std::println(stdout, "  -e, --expression=SCRIPT    script");
  std::println(stdout, "  -f, --file=FILE            script file");
  std::println(stdout, "  -n, --quiet, --silent      suppress default print");
  std::println(stdout, "  -E, -r, --regexp-extended  extended regex");
  std::println(stdout, "  -s, --separate             treat files separately");
  std::println(stdout, "  -z, --null-data            NUL-delimited lines");
  std::println(stdout, "  -i[SUFFIX], --inplace[=SUFFIX]  edit files in place");
  std::println(stdout, "  --sandbox                  disable e/r/w commands");
  std::println(stdout, "  --help                     show this help");
}

// Reads a whole file into a string; die()s on failure.
string slurp_file(const string &path) {
  std::ifstream ifs(path);
  if (!ifs)
    die(std::format("cannot open script file '{}'", path));
  std::ostringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

// Parser state threaded through the argv walk below.
struct ParseState {
  Options opts;
  vector<string> expressions;
  bool have_expr_or_file = false;
};

// Consumes the value for a required-value option (-e/-f, long or
// short). `rest` is whatever followed the option letter/name in the
// same token (may be empty); if empty, the value is taken from the
// next argv entry.
string take_value(ParseState &, const string &opt_name, string_view rest,
                   int argc, char **argv, int &i) {
  if (!rest.empty())
    return string(rest);
  if (i + 1 >= argc)
    die(std::format("option '{}' requires an argument", opt_name));
  return argv[++i];
}

// Handles one clustered short-option token, e.g. "-ne" or "-i.bak".
// `body` is everything after the leading '-'.
void parse_short_cluster(ParseState &st, string_view body, int argc,
                          char **argv, int &i) {
  for (size_t k = 0; k < body.size(); ++k) {
    const char c = body[k];
    switch (c) {
    case 'n':
      st.opts.suppress = true;
      break;
    case 'E':
    case 'r':
      st.opts.extended_re = true;
      break;
    case 's':
      st.opts.separate = true;
      break;
    case 'z':
      st.opts.null_delim = true;
      break;
    case 'e': {
      string v = take_value(st, "-e", body.substr(k + 1), argc, argv, i);
      st.expressions.push_back(std::move(v));
      st.have_expr_or_file = true;
      return; // rest of token (if any) was consumed as the value
    }
    case 'f': {
      string v = take_value(st, "-f", body.substr(k + 1), argc, argv, i);
      st.expressions.push_back(slurp_file(v));
      st.have_expr_or_file = true;
      return;
    }
    case 'i': {
      string_view rest = body.substr(k + 1);
      st.opts.inplace_suf = rest.empty() ? string(1, kBareInplace) : string(rest);
      return; // -i's value is glued-only; never consumes the next argv
    }
    default:
      die(std::format("unrecognised option '-{}'", c));
    }
  }
}

// Handles one "--name" / "--name=value" token (name excludes "--").
void parse_long_option(ParseState &st, string_view token, int argc,
                        char **argv, int &i) {
  const auto eq = token.find('=');
  const string_view name = token.substr(0, eq);
  const bool has_value = eq != string_view::npos;
  const string_view value = has_value ? token.substr(eq + 1) : string_view{};

  auto require_value = [&](const char *opt_name) {
    if (has_value)
      return string(value);
    if (i + 1 >= argc)
      die(std::format("option '--{}' requires an argument", opt_name));
    return string(argv[++i]);
  };

  if (name == "expression") {
    st.expressions.push_back(require_value("expression"));
    st.have_expr_or_file = true;
  } else if (name == "file") {
    st.expressions.push_back(slurp_file(require_value("file")));
    st.have_expr_or_file = true;
  } else if (name == "quiet" || name == "silent") {
    st.opts.suppress = true;
  } else if (name == "regexp-extended" || name == "regexp") {
    st.opts.extended_re = true;
  } else if (name == "separate") {
    st.opts.separate = true;
  } else if (name == "null-data") {
    st.opts.null_delim = true;
  } else if (name == "sandbox") {
    st.opts.sandbox = true;
  } else if (name == "inplace") {
    // Optional value: glued ("--inplace=.bak") only, per GNU sed —
    // "--inplace foo" treats foo as the script/file, not a suffix.
    st.opts.inplace_suf = has_value ? string(value) : string(1, kBareInplace);
  } else if (name == "help") {
    print_usage();
    std::exit(0);
  } else {
    die(std::format("unrecognised option '--{}'", name));
  }
}

} // namespace

Options parse_args(int argc, char **argv) {
  ParseState st;
  bool no_more_options = false;

  for (int i = 1; i < argc; ++i) {
    const string_view arg = argv[i];

    if (no_more_options || arg == "-" || arg.empty() || arg[0] != '-') {
      st.opts.files.push_back(string(arg));
      continue;
    }

    if (arg == "--") {
      no_more_options = true;
      continue;
    }

    if (arg.size() >= 2 && arg[1] == '-') {
      parse_long_option(st, arg.substr(2), argc, argv, i);
    } else {
      parse_short_cluster(st, arg.substr(1), argc, argv, i);
    }
  }

  // ── Assemble script text ──────────────────────────────────
  string combined;
  for (const string &e : st.expressions) {
    if (!combined.empty())
      combined += '\n';
    combined += e;
  }

  if (!st.have_expr_or_file) {
    // Bare positional argument as script if no -e/-f given.
    if (st.opts.files.empty())
      die("no script specified (use -e or -f)");
    combined = st.opts.files.front();
    st.opts.files.erase(st.opts.files.begin());
  }
  // else: every positional argument collected above is an input file.

  st.opts.scripts.push_back(std::move(combined));

  // ── Detect #n on first line ───────────────────────────────
  {
    Parser probe;
    probe.src = st.opts.scripts[0];
    st.opts.first_hash_n = probe.first_line_is_hash_n();
  }

  return st.opts;
}

} // namespace fastsed
