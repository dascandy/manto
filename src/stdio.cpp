#include "async/file.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

namespace {
  file& in() {
    static file in_(0);
    return in_;
  }
  file& out() {
    static file out_(1);
    return out_;
  }
  file& err() {
    static file err_(2);
    return err_;
  }
}

future<ssize_t> stdio::read(uint8_t* p, size_t count) {
  return in().read(p, count, 0);
}

future<ssize_t> stdio::read(char* p, size_t count) {
  return in().read(reinterpret_cast<uint8_t*>(p), count, 0);
}

future<ssize_t> stdio::write(std::span<const uint8_t> msg) {
  return out().write(msg, 0);
}

future<ssize_t> stdio::write(std::string_view msg) {
  return out().write(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()), 0);
}

future<ssize_t> stdio::write(std::u8string_view msg) {
  return out().write(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()), 0);
}

future<ssize_t> stdio::write(const char* msg) {
  return out().write(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(msg), strlen(msg)), 0);
}

future<ssize_t> stdio::error(std::span<const uint8_t> msg) {
  return err().write(msg, 0);
}

future<ssize_t> stdio::error(std::u8string_view msg) {
  return err().write(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()), 0);
}

future<ssize_t> stdio::error(std::string_view msg) {
  return err().write(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()), 0);
}

future<ssize_t> stdio::error(const char* msg) {
  return err().write(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(msg), strlen(msg)), 0);
}

file::file(int fd, Mode mode)
: mode(mode)
, fd(fd)
{
}

future<file> file::create(const std::string& filename, Mode mode)
{
  int m = O_LARGEFILE | O_CLOEXEC;
  switch(mode) {
    case Mode::ExclusiveNew: m |= O_RDWR | O_CREAT | O_EXCL; break;
    case Mode::Append: m |= O_WRONLY | O_CREAT | O_APPEND; break;
    case Mode::Truncate: m |= O_RDWR | O_CREAT | O_TRUNC; break;
    case Mode::Overwrite: m |= O_RDWR | O_CREAT; break;
    case Mode::Readonly: m |= O_RDONLY; break;
  }
  int fd = co_await async_openat(AT_FDCWD, filename.c_str(), 0, m);
  co_return file(fd, mode);
}

file::file(file&& rhs) {
  fd = rhs.fd;
  rhs.fd = -1;
}

file& file::operator=(file&& rhs) {
  fd = rhs.fd;
  rhs.fd = -1;
  return *this;
}

file::~file() {
  // async destructor!! darn it
  if (fd > 2) close(fd);
}

future<ssize_t> file::read(uint8_t* p, size_t count, ssize_t offset) {
  co_return co_await async_read(fd, p, count, offset == -1 ? currentOffset : offset);
}

future<ssize_t> file::write(std::span<const uint8_t> msg, ssize_t offset) {
  co_return co_await async_write(fd, (void*)msg.data(), msg.size(), offset == -1 ? currentOffset : offset);
}

file::mapping::~mapping() {
  munmap(p, length);
}

std::span<uint8_t> file::mapping::region() {
  return {p, p + length};
}

[[nodiscard]] file::mapping file::map(size_t start, size_t length) {
  static size_t pagesize = sysconf(_SC_PAGE_SIZE);
  size_t length_to_page = ((length + pagesize - 1) / pagesize) * pagesize;
  void* p = mmap(nullptr, length_to_page, PROT_READ | (mode == Mode::Readonly ? 0 : PROT_WRITE), MAP_SHARED, fd, start);
  return {(uint8_t*)p, length_to_page};
}

