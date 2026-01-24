#pragma once

#include <string>
#include <vector>

namespace shell {

// Check if a command is a builtin
bool is_builtin(const std::string& command);

// Execute a builtin command
// Returns true if the command was handled (whether successful or not)
// Returns false if it's not a builtin
bool run_builtin(const std::vector<std::string>& args);

// Individual builtin implementations
void builtin_cd(const std::string& path);
void builtin_pwd();
void builtin_echo(const std::vector<std::string>& args);
void builtin_type(const std::string& arg);
void builtin_history(const std::vector<std::string>& args);
void builtin_exit();

} // namespace shell
