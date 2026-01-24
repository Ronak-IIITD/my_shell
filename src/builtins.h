#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace shell {

// Check if a command is a builtin
bool is_builtin(std::string_view command) noexcept;

// Execute a builtin command
// Returns exit code: 0 for success, non-zero for failure
// Returns -1 if not a builtin (shouldn't happen if is_builtin checked first)
int run_builtin(const std::vector<std::string>& args);

// Individual builtin implementations - all return exit codes
int builtin_cd(std::string_view path);
int builtin_pwd();
int builtin_echo(const std::vector<std::string>& args);
int builtin_type(std::string_view arg);
int builtin_history(const std::vector<std::string>& args);
void builtin_exit() noexcept;  // This one actually exits, so void is appropriate

} // namespace shell
