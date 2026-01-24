#pragma once

#include <string>

namespace shell {

// Find the full path to an executable by searching PATH environment variable
// Returns empty string if command is not found
std::string get_path(const std::string& command);

} // namespace shell
