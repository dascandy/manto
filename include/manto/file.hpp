#pragma once

#include "manto/async_syscall.hpp"
#include "manto/future.hpp"
#include <span>
#include <unistd.h>

struct file {
  enum class Mode {
    Readonly,
    ExclusiveNew,
    Append,
    Truncate,
    Overwrite,
  } mode;
  file(int fd = -1, Mode mode = Mode::Readonly);
  static future<file> create(const std::string& filename, Mode mode = Mode::Readonly);
  file(file&& rhs);
  file& operator=(file&& rhs);
  ~file();
  future<ssize_t> read(uint8_t* p, size_t count, ssize_t offset);
  future<ssize_t> write(std::span<const uint8_t> msg, ssize_t offset);
  struct mapping {
    ~mapping();
    uint8_t* p;
    size_t length;
    std::span<uint8_t> region();
  };
  [[nodiscard]] mapping map(size_t start, size_t length);
  int fd;
  size_t currentOffset = 0;
};

struct stdio {
  static future<ssize_t> read(uint8_t* p, size_t count);
  static future<ssize_t> read(char* p, size_t count);
  static future<ssize_t> write(std::span<const uint8_t> msg);
  static future<ssize_t> write(std::string_view msg);
  static future<ssize_t> write(std::u8string_view msg);
  static future<ssize_t> write(const char* msg);
  static future<ssize_t> error(std::span<const uint8_t> msg);
  static future<ssize_t> error(std::string_view msg);
  static future<ssize_t> error(std::u8string_view msg);
  static future<ssize_t> error(const char* msg);
};


