#include "utils.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <unistd.h>

namespace fs = std::filesystem;

namespace shell {

std::string get_path(const std::string& command) {
  const char* get_env_cstr = std::getenv("PATH");
  if (get_env_cstr == nullptr) {
    return ""; // PATH not set!, can't find command
  }
  std::string path_env = get_env_cstr;
  std::stringstream ss(path_env);
  std::string path_dir;

  while (std::getline(ss, path_dir, ':')) {
    fs::path full_path = path_dir;
    full_path /= command; // append command to directory (e.g., /usr/bin/ + ls)

    // Check if file exists and is executable
    if (fs::exists(full_path) && access(full_path.c_str(), X_OK) == 0) {
      return full_path.string();
    }
  }
  return "";
}

// Consistent error reporting
void print_error(const std::string& command, const std::string& message) {
  std::cerr << command << ": " << message << std::endl;
}

void print_error(const std::string& message) {
  std::cerr << "shell: " << message << std::endl;
}

} // namespace shell
