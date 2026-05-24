// ============================================================
//  LineSource.cpp  —  unified line reader
//  Files   → Boost.Iostreams mapped_file_source (zero-copy)
//  stdin   → fgetc streaming
//  Both    → one-line lookahead for accurate last_line
// ============================================================

#include "fastsed/LineSource.hpp"
#include <sys/mman.h>
#include <sys/stat.h>

namespace fastsed {

void LineSource::set_delim(char d) { delim_ = d; }
void LineSource::set_separate(bool s) { separate_ = s; }

void LineSource::add_stdin() {
  Seg seg;
  seg.fname = "(stdin)";
  seg.is_pipe = true;
  segs_.push_back(std::move(seg));
}

void LineSource::add_file(const string &path) {
  if (path == "-") {
    add_stdin();
    return;
  }

  Seg seg;
  seg.fname = path;

  try {
    // Boost.Iostreams mapped_file_source cannot map a 0-byte file.
    // Stat first; if the file is empty just leave cur==end==nullptr
    // and the segment will be skipped naturally in read_raw().
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0)
      die(std::format("cannot stat '{}': {}", path, strerror(errno)));

    if (st.st_size > 0) {
      bio::mapped_file_params params;
      params.path = path;
      params.flags = bio::mapped_file::readonly;
      seg.mf.open(params);
      if (!seg.mf.is_open())
        die(std::format("cannot open '{}'", path));
      seg.cur = seg.mf.data();
      seg.end = seg.cur + seg.mf.size();
    }
    // else: empty file — cur and end remain nullptr, read_raw skips it
  } catch (const std::exception &e) {
    die(std::format("cannot map '{}': {}", path, e.what()));
  }

  segs_.push_back(std::move(seg));
}

// ── Raw read (no lookahead) ───────────────────────────────────
bool LineSource::read_raw(string &line, string &fname) {
  while (si_ < static_cast<int>(segs_.size())) {
    Seg &seg = segs_[si_];

    if (seg.is_pipe) {
      line.clear();
      bool got = false;
      int c;
      while ((c = fgetc(stdin)) != EOF && c != static_cast<int>(delim_)) {
        line += static_cast<char>(c);
        got = true;
      }
      if (c == static_cast<int>(delim_))
        got = true;
      if (got) {
        fname = seg.fname;
        return true;
      }
      ++si_;
      continue;
    }

    if (!seg.mf.is_open() || seg.cur >= seg.end) {
      ++si_;
      continue;
    }

    const char *nl = static_cast<const char *>(
        memchr(seg.cur, delim_, static_cast<size_t>(seg.end - seg.cur)));

    if (nl) {
      line.assign(seg.cur, nl);
      seg.cur = nl + 1;
    } else {
      line.assign(seg.cur, seg.end);
      seg.cur = seg.end;
    }
    fname = seg.fname;
    return true;
  }
  return false;
}

// ── Lookahead ─────────────────────────────────────────────────
bool LineSource::prime() {
  if (have_peek_)
    return !peeked_eof_;
  have_peek_ = true;
  peeked_eof_ = !read_raw(peeked_line_, peeked_fname_);
  return !peeked_eof_;
}

bool LineSource::has_more() { return prime() && !peeked_eof_; }

bool LineSource::read(string &line, string &fname, bool &is_last) {
  if (!prime() || peeked_eof_)
    return false;

  line = std::move(peeked_line_);
  fname = std::move(peeked_fname_);
  have_peek_ = false;

  prime(); // peek at next to determine is_last

  if (separate_) {
    // In -s mode: last of file if next line is from a different file
    is_last = peeked_eof_ || (!peeked_eof_ && peeked_fname_ != fname);
  } else {
    is_last = peeked_eof_;
  }
  return true;
}

} // namespace fastsed
