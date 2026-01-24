#pragma once

#include <string>
#include <vector>

namespace shell {

// Initialize history management
void init_history();

// Load history from HISTFILE environment variable
void load_history_from_file();

// Save new history entries to file
void save_history_to_file();

// Add command to history
void add_to_history(const std::string& command);

// Get reference to command history vector
std::vector<std::string>& get_command_history();

// Get the index of last saved history item
size_t get_history_file_index();

// Set the history file index (used after loading from file)
void set_history_file_index(size_t index);

} // namespace shell
