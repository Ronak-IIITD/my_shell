#pragma once

#include <string>
#include <vector>

namespace shell {

// Execute an external command (forks and execs)
void execute_external(const std::string& path,
                      const std::vector<std::string>& parts);

// Execute a command (builtin or external) with proper logic
// Returns true if it was a builtin, false otherwise
bool execute_command(const std::vector<std::string>& args);

// Execute a pipeline of commands (e.g., cmd1 | cmd2 | cmd3)
void execute_pipeline(const std::vector<std::vector<std::string>>& commands);

} // namespace shell
