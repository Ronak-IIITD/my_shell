#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <unistd.h>

namespace shell {

// Find the full path to an executable by searching PATH environment variable
// Returns empty string if command is not found
std::string get_path(std::string_view command) noexcept;

// Error reporting utilities for consistent error messages
void print_error(std::string_view command, std::string_view message) noexcept;
void print_error(std::string_view message) noexcept;

// Exit codes
constexpr int EXIT_SUCCESS_CODE = 0;
constexpr int EXIT_FAILURE_CODE = 1;
constexpr int EXIT_COMMAND_NOT_FOUND = 127;

// RAII wrapper for file descriptors
// Automatically closes fd on destruction (following RAII principles)
class FileDescriptor {
public:
  // Default constructor - invalid fd
  FileDescriptor() noexcept : fd_(-1) {}
  
  // Constructor from existing fd
  explicit FileDescriptor(int fd) noexcept : fd_(fd) {}
  
  // Disable copy (file descriptors shouldn't be copied)
  FileDescriptor(const FileDescriptor&) = delete;
  FileDescriptor& operator=(const FileDescriptor&) = delete;
  
  // Enable move semantics
  FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
  }
  
  FileDescriptor& operator=(FileDescriptor&& other) noexcept {
    if (this != &other) {
      close();
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }
  
  // Destructor - automatically close fd
  ~FileDescriptor() noexcept {
    close();
  }
  
  // Get the raw fd
  int get() const noexcept { return fd_; }
  
  // Check if fd is valid
  bool is_valid() const noexcept { return fd_ >= 0; }
  
  // Explicit bool conversion
  explicit operator bool() const noexcept { return is_valid(); }
  
  // Release ownership (caller takes responsibility for closing)
  int release() noexcept {
    int fd = fd_;
    fd_ = -1;
    return fd;
  }
  
  // Close the fd
  void close() noexcept {
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }
  
private:
  int fd_;
};

// Custom deleter for readline strings
struct ReadlineDeleter {
  void operator()(char* ptr) const noexcept {
    if (ptr) {
      free(ptr);
    }
  }
};

// Smart pointer type for readline strings
using ReadlinePtr = std::unique_ptr<char, ReadlineDeleter>;

} // namespace shell
