#include "completion.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <readline/readline.h>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace shell {

// List of builtin commands for completion
static const std::vector<std::string> builtin_commands = {
    "echo", "exit", "type", "pwd", "cd", "history"};

// The Generator
// Readline calls this repeatedly. state is 0 the first time, then 1, 2, etc.
// We return the next match, or nullptr when done.
char* command_generator(const char* text, int state) {
  // Static variables persist between calls during the same TAB session
  static std::vector<std::string> matches;
  static size_t match_index = 0;

  // State 0 = first call for this TAB press. We must find all matches now
  if (state == 0) {
    matches.clear();
    match_index = 0;

    std::string text_str(text);
    std::set<std::string> found; // to avoid duplicates

    // 1) search builtin_commands
    for (const auto& cmd : builtin_commands) {
      // check if builtin starts with 'text'
      if (cmd.find(text_str) == 0) {
        matches.push_back(cmd);
        found.insert(cmd);
      }
    }
    // 2) search for path executable
    const char* path_env = std::getenv("PATH");
    if (path_env) {
      std::string path_str = path_env;
      std::stringstream ss(path_str);
      std::string dir;

      while (std::getline(ss, dir, ':')) {
        fs::path p(dir);

        // gracefully handle directories that don't exists
        if (!fs::exists(p) || !fs::is_directory(p))
          continue;

        // iterate over files in the directory
        try {
          for (const auto& entry : fs::directory_iterator(p)) {
            // skip if not a regular file (eg: skip directories/ sockets inside
            // bin)
            if (!entry.is_regular_file())
              continue;
            std::string filename = entry.path().filename().string();

            // check if filename starts with what user typed
            if (filename.find(text_str) == 0) {
              // check if file is executable (X_OK)
              // this filters out README's or other non-program files
              if (access(entry.path().c_str(), X_OK) == 0) {
                // only add if we haven't seen this command yet
                if (found.find(filename) == found.end()) {
                  matches.push_back(filename);
                  found.insert(filename);
                }
              }
            }
          }
        } catch (...) {
          // ignore permission errors (eg: if we can't read a directory)
        }
      }
    }
  }
  // return the next match on our list
  if (match_index < matches.size()) {
    // readline requires us to return a malloc'd string (strdup does this)
    return strdup(matches[match_index++].c_str());
  }
  // no more matches
  return nullptr;
}

// The Hook
// This function tells readline: "if the user hits TAB at the start of the
// line, use our custom Generator."
char** shell_completion(const char* text, int start, int end) {
  // only AUTOCOMPLETE the first word (the command)
  if (start == 0) {
    // prevent readline from falling back to filename completion
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, command_generator);
  }
  return nullptr;
}

void init_completion() {
  // Tell readline to use our custom function when TAB is pressed
  rl_attempted_completion_function = shell_completion;
}

} // namespace shell
