#pragma once
// ============================================================
//  LineSource.hpp  —  unified line reader
//
//  Regular files are memory-mapped via Boost.Iostreams
//  mapped_file_source for zero-copy sequential access.
//  stdin / pipes fall back to fgetc-based streaming.
//  One-line lookahead provides accurate last_line detection
//  without reading the entire file up front.
// ============================================================

#include "Common.hpp"
#include <boost/iostreams/device/mapped_file.hpp>

namespace fastsed {
namespace bio = boost::iostreams;

class LineSource {
public:
  LineSource() = default;
  ~LineSource() = default;
  LineSource(const LineSource &) = delete;

  void set_delim(char d);    // default '\n'; -z uses '\0'
  void set_separate(bool s); // -s: treat each file independently

  void add_file(const string &path); // "-" → stdin
  void add_stdin();

  // Returns true while lines remain.
  bool has_more();

  // Fills line / fname / is_last; advances internal cursor.
  bool read(string &line, string &fname, bool &is_last);

private:
  // ── Per-segment state ─────────────────────────────────────
  struct Seg {
    string fname;
    bool is_pipe = false;
    // mmap-backed
    bio::mapped_file_source mf;
    const char *cur = nullptr;
    const char *end = nullptr;
  };

  vector<Seg> segs_;
  int si_ = 0;
  char delim_ = '\n';
  bool separate_ = false;

  // ── One-line lookahead ─────────────────────────────────────
  string peeked_line_, peeked_fname_;
  bool have_peek_ = false;
  bool peeked_eof_ = false;

  bool read_raw(string &line, string &fname);
  bool prime();
};

} // namespace fastsed
