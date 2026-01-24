#pragma once

#include <string>
#include <vector>

namespace shell {

struct RedirectionInfo {
  std::string filename;
  int target_fd; // 1 for stdout, 2 for stderr, -1 for none
  bool append;   // true=append (>>), false=overwrite (>)
};

// Parse redirection operators from args and remove them from the vector
// Returns RedirectionInfo with target_fd=-1 if no redirection found
RedirectionInfo parse_redirection(std::vector<std::string>& args);

// Apply redirection to file descriptor
// Returns the saved file descriptor for later restoration, or -1 if no redirection
int apply_redirection(const RedirectionInfo& redir);

// Restore original file descriptor
void restore_redirection(int saved_fd, int target_fd);

} // namespace shell
