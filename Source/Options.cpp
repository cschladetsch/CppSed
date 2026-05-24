// ============================================================
//  Options.cpp  —  CLI argument parsing via Boost.Program_options
// ============================================================

#include "fastsed/Options.hpp"
#include "fastsed/Parser.hpp"
#include "fastsed/Print.hpp"

#include <boost/program_options.hpp>
#include <fstream>
#include <sstream>

namespace fastsed {
namespace po = boost::program_options;

Options parse_args(int argc, char **argv) {
  Options opts;

  // ── Declare options ───────────────────────────────────────
  po::options_description visible("Options");
  visible.add_options()("expression,e",
                        po::value<vector<string>>()->composing(), "script")(
      "file,f", po::value<vector<string>>()->composing(), "script file")(
      "quiet,n", po::bool_switch(&opts.suppress), "suppress default print")(
      "silent", po::bool_switch(&opts.suppress), "alias for -n")(
      "regexp-extended,E", po::bool_switch(&opts.extended_re),
      "extended regex")("separate,s", po::bool_switch(&opts.separate),
                        "treat files separately")(
      "null-data,z", po::bool_switch(&opts.null_delim), "NUL-delimited lines")(
      "sandbox", po::bool_switch(&opts.sandbox),
      "disable e/r/w commands")("help", "show this help");

  // -r is a synonym for -E
  po::options_description hidden;
  hidden.add_options()("regexp,r", po::bool_switch(&opts.extended_re))(
      "inplace,i", po::value<string>()->implicit_value(""),
      "-i[SUFFIX]")("input-file", po::value<vector<string>>(), "input files");

  po::options_description all;
  all.add(visible).add(hidden);

  po::positional_options_description pos;
  pos.add("input-file", -1);

  // ── Parse ─────────────────────────────────────────────────
  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv)
                  .options(all)
                  .positional(pos)
                  .style(po::command_line_style::default_style |
                         po::command_line_style::allow_long_disguise)
                  .run(),
              vm);
    po::notify(vm);
  } catch (const po::error &e) {
    die(std::format("argument error: {}", e.what()));
  }

  if (vm.count("help")) {
    std::println(stdout, "Usage: sed [OPTION]... SCRIPT [FILE]...");
    std::println(stdout, "       sed [OPTION]... -e SCRIPT... [FILE]...");
    std::println(stdout, "       sed [OPTION]... -f SCRIPTFILE... [FILE]...");
    std::println(stdout, "{}", [&] {
      std::ostringstream os;
      os << visible;
      return os.str();
    }());
    std::exit(0);
  }

  // ── In-place suffix ───────────────────────────────────────
  if (vm.count("inplace"))
    opts.inplace_suf = vm["inplace"].as<string>();

  // ── Assemble script text ──────────────────────────────────
  string combined;

  // Inline -e scripts
  if (vm.count("expression")) {
    for (const string &e : vm["expression"].as<vector<string>>()) {
      if (!combined.empty())
        combined += '\n';
      combined += e;
    }
  }

  // File-based -f scripts
  if (vm.count("file")) {
    for (const string &path : vm["file"].as<vector<string>>()) {
      std::ifstream ifs(path);
      if (!ifs)
        die(std::format("cannot open script file '{}'", path));
      std::ostringstream ss;
      ss << ifs.rdbuf();
      if (!combined.empty())
        combined += '\n';
      combined += ss.str();
    }
  }

  // Bare positional argument as script if no -e/-f given
  if (combined.empty()) {
    const auto *pos_files = vm.count("input-file")
                                ? &vm["input-file"].as<vector<string>>()
                                : nullptr;
    if (!pos_files || pos_files->empty())
      die("no script specified (use -e or -f)");
    combined = (*pos_files)[0];
    // remaining positional args are the input files
    for (size_t i = 1; i < pos_files->size(); ++i)
      opts.files.push_back((*pos_files)[i]);
  } else if (vm.count("input-file")) {
    opts.files = vm["input-file"].as<vector<string>>();
  }

  opts.scripts.push_back(std::move(combined));

  // ── Detect #n on first line ───────────────────────────────
  {
    Parser probe;
    probe.src = opts.scripts[0];
    opts.first_hash_n = probe.first_line_is_hash_n();
  }

  return opts;
}

} // namespace fastsed
