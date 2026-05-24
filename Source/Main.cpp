// ============================================================
//  Main.cpp  —  entry point
// ============================================================

#include "fastsed/Common.hpp"
#include "fastsed/Engine.hpp"
#include "fastsed/LineSource.hpp"
#include "fastsed/Linker.hpp"
#include "fastsed/Options.hpp"
#include "fastsed/OutBuf.hpp"
#include "fastsed/Parser.hpp"

#include <fcntl.h>
#include <sys/stat.h>

namespace fastsed {

// ── In-place file guard ───────────────────────────────────────
struct InplaceGuard {
  string orig, tmp;
  int tmp_fd = -1;

  ~InplaceGuard() {
    if (tmp_fd >= 0) {
      ::close(tmp_fd);
      ::unlink(tmp.c_str());
    }
  }

  void begin(const string &path) {
    orig = path;
    tmp = path + ".fastsed_tmp";
    tmp_fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (tmp_fd < 0)
      die(std::format("cannot create '{}': {}", tmp, strerror(errno)));
    g_out.flush();
    g_out.fd = tmp_fd;
  }

  void commit(const string &suffix) {
    g_out.flush();
    ::close(tmp_fd);
    tmp_fd = -1;
    g_out.fd = STDOUT_FILENO;
    if (!suffix.empty()) {
      string bak = orig + suffix;
      if (::rename(orig.c_str(), bak.c_str()) != 0)
        die(std::format("rename: {}", strerror(errno)));
    }
    if (::rename(tmp.c_str(), orig.c_str()) != 0)
      die(std::format("rename: {}", strerror(errno)));
    tmp.clear();
  }
};

// ── Process a single LineSource through the engine ────────────
static void run_engine(Engine &eng, LineSource &src) {
  eng.process(src);
  g_out.flush();
}

} // namespace fastsed

// ─────────────────────────────────────────────────────────────
int main(int argc, char **argv) {
  using namespace fastsed;

  Options opts = parse_args(argc, argv);
  if (opts.first_hash_n)
    opts.suppress = true;

  // ── Compile script ────────────────────────────────────────
  Parser parser;
  parser.src = opts.scripts[0];
  parser.ext = opts.extended_re;
  CmdVec tree = parser.parse_cmds();

  Linker linker;
  linker.flatten(tree);
  linker.resolve();

  // ── Configure engine ──────────────────────────────────────
  Engine eng;
  eng.prog = &linker.prog;
  eng.suppress = opts.suppress;
  eng.sandbox = opts.sandbox;
  eng.separate = opts.separate;
  eng.init(static_cast<int>(linker.prog.size()));

  const char delim = opts.null_delim ? '\0' : '\n';
  const bool do_inplace = !opts.inplace_suf.empty();

  // ── No input files → read stdin ───────────────────────────
  if (opts.files.empty()) {
    LineSource src;
    src.set_delim(delim);
    src.set_separate(opts.separate);
    src.add_stdin();
    run_engine(eng, src);
    return g_exit_code;
  }

  // ── File input ────────────────────────────────────────────
  if (!do_inplace) {
    // All files as one stream
    LineSource src;
    src.set_delim(delim);
    src.set_separate(opts.separate);
    for (const string &f : opts.files)
      src.add_file(f);
    run_engine(eng, src);
  } else {
    // In-place: process each file independently
    for (const string &f : opts.files) {
      // Reset engine state between files
      eng.ps.clear();
      eng.hs.clear();
      eng.lineno = 0;
      eng.sub_made = false;
      eng.range_at.assign(static_cast<size_t>(eng.total_cmds), -1L);
      eng.deferred.clear();

      InplaceGuard guard;
      guard.begin(f);

      LineSource src;
      src.set_delim(delim);
      src.set_separate(false); // already per-file
      src.add_file(f);
      eng.process(src);

      guard.commit(opts.inplace_suf == "\x01" ? "" : opts.inplace_suf);
    }
  }

  return g_exit_code;
}
