#pragma once

#include <string>
#include <vector>

namespace shell {

// Check if a command is a builtin
bool is_builtin(const std::string& command);

// Execute a builtin command
// Returns exit code: 0 for success, non-zero for failure
// Returns -1 if not a builtin (shouldn't happen if is_builtin checked first)
int run_builtin(const std::vector<std::string>& args);

// Individual builtin implementations - all return exit codes
int builtin_cd(const std::string& path);
int builtin_pwd();
int builtin_echo(const std::vector<std::string>& args);
int builtin_type(const std::string& arg);
int builtin_history(const std::vector<std::string>& args);
void builtin_exit();  // This one actually exits, so void is appropriate

} // namespace shell
