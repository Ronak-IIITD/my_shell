#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace shell {

// Split input line into tokens, handling single quotes, double quotes, and escapes
// Example: 'echo "hello world"' -> ["echo", "hello world"]
std::vector<std::string> split_line(std::string_view input);

// Parse a command line into separate commands for pipeline execution
// Example: "ls -la | grep cpp | wc -l" -> [["ls", "-la"], ["grep", "cpp"], ["wc", "-l"]]
std::vector<std::vector<std::string>> parse_pipeline_args(const std::vector<std::string>& args);

} // namespace shell
