#pragma once
// ============================================================
//  TestHelper.hpp  —  test infrastructure for fastsed
//
//  run_sed()    compiles a script, runs it against a string,
//               captures g_out output, and returns the result.
//
//  Approach:
//    - Input  : written to mkstemp temp file, fed to LineSource
//    - Output : g_out.fd redirected to a pipe; drained after run
//    - Cleanup: temp file unlinked; g_out.fd restored; globals reset
// ============================================================

#include "fastsed/Common.hpp"
#include "fastsed/Engine.hpp"
#include "fastsed/LineSource.hpp"
#include "fastsed/Linker.hpp"
#include "fastsed/OutBuf.hpp"
#include "fastsed/Parser.hpp"

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace fastsed::test {

// ── RAII pipe wrapper ─────────────────────────────────────────
struct Pipe {
  int r = -1, w = -1;
  Pipe() {
    int fd[2];
    if (pipe(fd))
      throw std::runtime_error("pipe");
    r = fd[0];
    w = fd[1];
  }
  ~Pipe() {
    if (r >= 0)
      close(r);
    if (w >= 0)
      close(w);
  }

  void close_write() {
    if (w >= 0) {
      close(w);
      w = -1;
    }
  }

  std::string drain() {
    close_write();
    std::string out;
    char buf[4096];
    ssize_t n;
    while ((n = read(r, buf, sizeof buf)) > 0)
      out.append(buf, static_cast<size_t>(n));
    return out;
  }
};

// ── Core helper ───────────────────────────────────────────────
inline std::string run_sed(const std::string &script, const std::string &input,
                           bool suppress = false, bool extended = false,
                           bool null_delim = false) {
  // Write input to a temporary file so LineSource can mmap it
  char tmppath[] = "/tmp/fastsed_test_XXXXXX";
  int tmpfd = mkstemp(tmppath);
  if (tmpfd < 0)
    throw std::runtime_error("mkstemp failed");
  if (!input.empty())
    (void)write(tmpfd, input.data(), input.size());
  close(tmpfd);

  struct Cleanup {
    const char *p;
    ~Cleanup() { unlink(p); }
  } cleanup{tmppath};

  // Redirect g_out to capture pipe
  Pipe cap;
  g_out.flush();
  int saved_fd = g_out.fd;
  g_out.fd = cap.w;
  g_out.n = 0;
  g_exit_code = 0;

  // Compile
  Parser p;
  p.src = script;
  p.ext = extended;
  CmdVec tree = p.parse_cmds();

  Linker lk;
  lk.flatten(tree);
  lk.resolve();

  // Run
  Engine eng;
  eng.prog = &lk.prog;
  eng.suppress = suppress;
  eng.init(static_cast<int>(lk.prog.size()));

  LineSource src;
  src.set_delim(null_delim ? '\0' : '\n');
  src.add_file(tmppath);
  eng.process(src);

  // Restore
  g_out.flush();
  g_out.fd = saved_fd;
  g_out.n = 0;
  g_exit_code = 0;

  return cap.drain();
}

// ── Convenience: split output string into lines ───────────────
inline std::vector<std::string> lines(const std::string &s) {
  std::vector<std::string> v;
  std::string cur;
  for (char c : s) {
    if (c == '\n') {
      v.push_back(cur);
      cur.clear();
    } else
      cur += c;
  }
  if (!cur.empty())
    v.push_back(cur);
  return v;
}

// ── Convenience: join lines with newlines (adds trailing \n) ──
inline std::string join(std::initializer_list<std::string_view> ls) {
  std::string s;
  for (auto l : ls) {
    s += l;
    s += '\n';
  }
  return s;
}

} // namespace fastsed::test
