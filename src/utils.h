#pragma once

#include <string>

namespace shell {

// Find the full path to an executable by searching PATH environment variable
// Returns empty string if command is not found
std::string get_path(const std::string& command);

// Error reporting utilities for consistent error messages
void print_error(const std::string& command, const std::string& message);
void print_error(const std::string& message);

// Exit codes
constexpr int EXIT_SUCCESS_CODE = 0;
constexpr int EXIT_FAILURE_CODE = 1;
constexpr int EXIT_COMMAND_NOT_FOUND = 127;

} // namespace shell
