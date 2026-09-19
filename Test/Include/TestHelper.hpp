#pragma once
// ============================================================
//  TestHelper.hpp  —  test infrastructure for fastsed
//
//  run_sed()    compiles a script, runs it against a string,
//               captures g_out output, and returns the result.
//
//  Approach:
//    - Input  : written to a temp file, fed to LineSource
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

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fastsed::test {

// ── RAII pipe wrapper ─────────────────────────────────────────
struct Pipe {
  int r = -1, w = -1;
  Pipe() {
#if defined(_WIN32)
    int fd[2];
    // Binary mode: text-mode CRT pipes translate \n<->\r\n, which
    // would corrupt captured output byte-for-byte comparisons.
    if (_pipe(fd, 4096, _O_BINARY))
      throw std::runtime_error("pipe");
    r = fd[0];
    w = fd[1];
#else
    int fd[2];
    if (pipe(fd))
      throw std::runtime_error("pipe");
    r = fd[0];
    w = fd[1];
#endif
  }
  ~Pipe() {
#if defined(_WIN32)
    if (r >= 0)
      _close(r);
    if (w >= 0)
      _close(w);
#else
    if (r >= 0)
      close(r);
    if (w >= 0)
      close(w);
#endif
  }

  void close_write() {
    if (w >= 0) {
#if defined(_WIN32)
      _close(w);
#else
      close(w);
#endif
      w = -1;
    }
  }

  std::string drain() {
    close_write();
    std::string out;
    char buf[4096];
#if defined(_WIN32)
    int n;
    while ((n = _read(r, buf, sizeof buf)) > 0)
      out.append(buf, static_cast<size_t>(n));
#else
    ssize_t n;
    while ((n = read(r, buf, sizeof buf)) > 0)
      out.append(buf, static_cast<size_t>(n));
#endif
    return out;
  }
};

namespace detail {

// Creates a unique temp file, returns its path with the fd left open
// (matching mkstemp()'s contract) via out_fd.
inline std::string make_temp_file(int &out_fd) {
#if defined(_WIN32)
  char dir[MAX_PATH];
  DWORD dlen = ::GetTempPathA(sizeof dir, dir);
  if (dlen == 0 || dlen > sizeof dir)
    throw std::runtime_error("GetTempPathA failed");
  char path[MAX_PATH];
  if (::GetTempFileNameA(dir, "fsd", 0, path) == 0)
    throw std::runtime_error("GetTempFileNameA failed");
  int fd = -1;
  if (_sopen_s(&fd, path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC,
               _SH_DENYNO, _S_IREAD | _S_IWRITE) != 0 ||
      fd < 0)
    throw std::runtime_error("_sopen_s failed");
  out_fd = fd;
  return std::string(path);
#else
  char tmppath[] = "/tmp/fastsed_test_XXXXXX";
  int fd = mkstemp(tmppath);
  if (fd < 0)
    throw std::runtime_error("mkstemp failed");
  out_fd = fd;
  return std::string(tmppath);
#endif
}

} // namespace detail

// ── Core helper ───────────────────────────────────────────────
inline std::string run_sed(const std::string &script, const std::string &input,
                           bool suppress = false, bool extended = false,
                           bool null_delim = false) {
  // Write input to a temporary file so LineSource can mmap it
  int tmpfd = -1;
  std::string tmppath = detail::make_temp_file(tmpfd);

  if (!input.empty()) {
#if defined(_WIN32)
    (void)_write(tmpfd, input.data(), static_cast<unsigned int>(input.size()));
#else
    (void)write(tmpfd, input.data(), input.size());
#endif
  }
#if defined(_WIN32)
  _close(tmpfd);
#else
  close(tmpfd);
#endif

  struct Cleanup {
    std::string p;
    ~Cleanup() {
#if defined(_WIN32)
      _unlink(p.c_str());
#else
      unlink(p.c_str());
#endif
    }
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
