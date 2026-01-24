#include "history.h"
#include <cstdlib>
#include <fstream>
#include <ios>

namespace shell {

// Static storage for history
static std::vector<std::string> command_history;
static size_t history_file_index = 0;

void init_history() {
  command_history.clear();
  history_file_index = 0;
}

void load_history_from_file() {
  const char* histfile_env = std::getenv("HISTFILE");
  if (!histfile_env) {
    return; // HISTFILE not set
  }

  std::ifstream hist_file(histfile_env);
  if (hist_file.is_open()) {
    std::string line;
    while (std::getline(hist_file, line)) {
      if (!line.empty()) {
        command_history.push_back(line);
      }
    }
    hist_file.close();

    // Mark these lines as "already written"
    // so 'history -a' doesn't duplicate them later
    history_file_index = command_history.size();
  }
}

void save_history_to_file() {
  // Check if HISTFILE is set
  const char* histfile_env = std::getenv("HISTFILE");
  if (!histfile_env) {
    return; // Do nothing if variable isn't set
  }

  // Open file in APPEND mode
  std::ofstream hist_file(histfile_env, std::ios::app);

  if (hist_file.is_open()) {
    // Write only the new commands (from index to end)
    for (size_t i = history_file_index; i < command_history.size(); i++) {
      hist_file << command_history[i] << std::endl;
    }
    hist_file.close();
  }
}

void add_to_history(std::string_view command) {
  if (!command.empty()) {
    command_history.emplace_back(command);
  }
}

const std::vector<std::string>& get_command_history() noexcept {
  return command_history;
}

size_t get_history_file_index() noexcept {
  return history_file_index;
}

void set_history_file_index(size_t index) noexcept {
  history_file_index = index;
}

} // namespace shell
