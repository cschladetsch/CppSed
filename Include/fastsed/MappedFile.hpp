#pragma once
// ============================================================
//  MappedFile.hpp  —  minimal read-only memory-mapped file
//
//  Replaces boost::iostreams::mapped_file_source. POSIX uses
//  mmap(2); Windows uses CreateFileMapping/MapViewOfFile. Both
//  paths expose the same tiny interface LineSource needs:
//  open/is_open/data/size/close, RAII-closed on destruction.
// ============================================================

#include <string>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fastsed {

class MappedFile {
public:
  MappedFile() = default;
  MappedFile(const MappedFile &) = delete;
  MappedFile &operator=(const MappedFile &) = delete;

  MappedFile(MappedFile &&other) noexcept { *this = std::move(other); }
  MappedFile &operator=(MappedFile &&other) noexcept {
    if (this != &other) {
      close();
#if defined(_WIN32)
      file_ = other.file_;
      mapping_ = other.mapping_;
#else
      fd_ = other.fd_;
#endif
      data_ = other.data_;
      size_ = other.size_;
      other.reset_moved();
    }
    return *this;
  }

  ~MappedFile() { close(); }

  // Returns false (without throwing) on any failure; caller checks
  // is_open(). errorMessage() carries the reason for diagnostics.
  bool open(const std::string &path) {
    close();
    error_.clear();

#if defined(_WIN32)
    file_ = ::CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_ == INVALID_HANDLE_VALUE) {
      error_ = "cannot open file";
      file_ = nullptr;
      return false;
    }

    LARGE_INTEGER li{};
    if (!::GetFileSizeEx(file_, &li)) {
      error_ = "cannot stat file";
      close();
      return false;
    }
    size_ = static_cast<size_t>(li.QuadPart);

    if (size_ == 0) {
      // A 0-byte mapping is invalid on Windows too; leave data_ null,
      // is_open() reports false, LineSource treats it as an empty
      // segment (same behaviour as the previous Boost.Iostreams path).
      return true;
    }

    mapping_ = ::CreateFileMappingA(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping_) {
      error_ = "cannot map file";
      close();
      return false;
    }

    data_ = static_cast<const char *>(
        ::MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0));
    if (!data_) {
      error_ = "cannot map view of file";
      close();
      return false;
    }
    return true;

#else
    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
      error_ = "cannot open file";
      return false;
    }

    struct stat st{};
    if (::fstat(fd_, &st) != 0) {
      error_ = "cannot stat file";
      close();
      return false;
    }
    size_ = static_cast<size_t>(st.st_size);

    if (size_ == 0) {
      // mmap() rejects a zero-length mapping; same empty-segment
      // convention as the Windows branch above.
      return true;
    }

    void *mem = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (mem == MAP_FAILED) {
      error_ = "cannot mmap file";
      close();
      return false;
    }
    data_ = static_cast<const char *>(mem);
    return true;
#endif
  }

  void close() {
#if defined(_WIN32)
    if (data_)
      ::UnmapViewOfFile(data_);
    if (mapping_)
      ::CloseHandle(mapping_);
    if (file_)
      ::CloseHandle(file_);
    file_ = nullptr;
    mapping_ = nullptr;
#else
    if (data_ && size_ > 0)
      ::munmap(const_cast<char *>(data_), size_);
    if (fd_ >= 0)
      ::close(fd_);
    fd_ = -1;
#endif
    data_ = nullptr;
    size_ = 0;
  }

  [[nodiscard]] bool is_open() const { return data_ != nullptr; }
  [[nodiscard]] const char *data() const { return data_; }
  [[nodiscard]] size_t size() const { return size_; }
  [[nodiscard]] const std::string &error() const { return error_; }

private:
  void reset_moved() {
#if defined(_WIN32)
    file_ = nullptr;
    mapping_ = nullptr;
#else
    fd_ = -1;
#endif
    data_ = nullptr;
    size_ = 0;
  }

#if defined(_WIN32)
  void *file_ = nullptr;    // HANDLE
  void *mapping_ = nullptr; // HANDLE
#else
  int fd_ = -1;
#endif
  const char *data_ = nullptr;
  size_t size_ = 0;
  std::string error_;
};

} // namespace fastsed
