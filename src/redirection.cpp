#include "redirection.h"
#include "utils.h"
#include <fcntl.h>
#include <unistd.h>

namespace shell {

RedirectionInfo parse_redirection(std::vector<std::string>& args) {
  RedirectionInfo info = {"", -1, false}; // default: no redirection

  for (size_t i = 0; i < args.size(); i++) {
    // ">" operator to redirect standard output to a file
    if (args[i] == ">" || args[i] == "1>") {
      if (i + 1 < args.size()) {
        info.filename = args[i + 1];
        info.target_fd = 1; // stdout
        info.append = false;
        args.erase(args.begin() + i, args.begin() + i + 2);
        return info;
      }
    } else if (args[i] == ">>" || args[i] == "1>>") {
      if (i + 1 < args.size()) {
        info.filename = args[i + 1];
        info.target_fd = 1; // stdout
        info.append = true; // enable append mode
        args.erase(args.begin() + i, args.begin() + i + 2);
        return info;
      }
    } // standard error overwrite (2>)
    else if (args[i] == "2>") {
      if (i + 1 < args.size()) {
        info.filename = args[i + 1];
        info.target_fd = 2; // stderr
        info.append = false;
        args.erase(args.begin() + i, args.begin() + i + 2);
        return info;
      }
    } // standard error append (2>>)
    else if (args[i] == "2>>") {
      if (i + 1 < args.size()) {
        info.filename = args[i + 1];
        info.target_fd = 2; // stderr
        info.append = true;
        args.erase(args.begin() + i, args.begin() + i + 2);
        return info;
      }
    }
  }
  return info;
}

int apply_redirection(const RedirectionInfo& redir) {
  if (redir.target_fd == -1) {
    return -1; // no redirection
  }

  // Save original stream using RAII wrapper
  FileDescriptor saved_fd(dup(redir.target_fd));
  
  if (!saved_fd.is_valid()) {
    print_error("shell", "Failed to duplicate file descriptor");
    return -1;
  }

  // Determine flags: Write Only + Create + (Append OR Truncate)
  int flags = O_WRONLY | O_CREAT;
  if (redir.append) {
    flags |= O_APPEND;
  } else {
    flags |= O_TRUNC;
  }

  // Open file (0644 = rw-r--r--) using RAII wrapper
  FileDescriptor file_fd(open(redir.filename.c_str(), flags, 0644));

  if (!file_fd.is_valid()) {
    print_error("shell", redir.filename + ": No such file or directory");
    return -1;
  }

  // Apply Redirection
  dup2(file_fd.get(), redir.target_fd);
  // file_fd automatically closed here by RAII

  return saved_fd.release(); // Release ownership to caller
}

void restore_redirection(int saved_fd, int target_fd) {
  if (saved_fd != -1) {
    FileDescriptor fd(saved_fd); // RAII wrapper takes ownership
    dup2(fd.get(), target_fd);
    // fd automatically closed here by RAII destructor
  }
}

} // namespace shell
