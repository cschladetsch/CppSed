#pragma once
// ============================================================
//  OutBuf.hpp  —  64 KiB slab output buffer
//  Fully inline; avoids a per-line write() syscall.
// ============================================================

#include "Common.hpp"

#if defined(_WIN32)
#include <io.h>
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1 // same numeric convention as POSIX/the CRT
#endif
#else
#include <unistd.h>
#endif

namespace fastsed {

struct OutBuf {
  static constexpr size_t CAP = 1u << 16; // 64 KiB
  char buf[CAP];
  size_t n = 0;
  int fd = STDOUT_FILENO;

  explicit OutBuf(int fd = STDOUT_FILENO) : fd(fd) {}
  ~OutBuf() { flush(); }
  OutBuf(const OutBuf &) = delete;
  OutBuf &operator=(const OutBuf &) = delete;

  void flush() noexcept {
    for (size_t off = 0; off < n;) {
#if defined(_WIN32)
      int w = ::_write(fd, buf + off, static_cast<unsigned int>(n - off));
#else
      ssize_t w = ::write(fd, buf + off, n - off);
#endif
      if (w <= 0)
        break;
      off += static_cast<size_t>(w);
    }
    n = 0;
  }

  void put(char c) noexcept {
    buf[n++] = c;
    if (n == CAP)
      flush();
  }

  void write(string_view sv) noexcept {
    const char *p = sv.data();
    size_t rem = sv.size();
    while (rem) {
      size_t chunk = std::min(rem, CAP - n);
      std::memcpy(buf + n, p, chunk);
      n += chunk;
      p += chunk;
      rem -= chunk;
      if (n == CAP)
        flush();
    }
  }

  void writeln(string_view sv) noexcept {
    write(sv);
    put('\n');
  }

  void write_long(long v) noexcept {
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof buf, v);
    if (ec == std::errc())
      write({buf, static_cast<size_t>(ptr - buf)});
  }
};

// Primary output buffer — defined in Common.cpp
extern OutBuf g_out;

} // namespace fastsed
