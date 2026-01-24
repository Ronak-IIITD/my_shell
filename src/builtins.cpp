#include "builtins.h"
#include "history.h"
#include "utils.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ios>

namespace fs = std::filesystem;

namespace shell {

// List of builtin command names
static const std::vector<std::string> builtin_names = {
    "echo", "exit", "type", "pwd", "cd", "history"};

bool is_builtin(std::string_view command) noexcept {
  for (const auto& builtin : builtin_names) {
    if (command == builtin) {
      return true;
    }
  }
  return false;
}

int run_builtin(const std::vector<std::string>& args) {
  if (args.empty()) {
    return -1; // not a builtin
  }

  const std::string& command_name = args[0];

  if (command_name == "exit") {
    builtin_exit();
    return 0; // Never reached but satisfies compiler
  } else if (command_name == "echo") {
    return builtin_echo(args);
  } else if (command_name == "type") {
    if (args.size() > 1) {
      return builtin_type(args[1]);
    }
    return 0;
  } else if (command_name == "pwd") {
    return builtin_pwd();
  } else if (command_name == "cd") {
    if (args.size() >= 2) {
      return builtin_cd(args[1]);
    } else {
      return builtin_cd("~");
    }
  } else if (command_name == "history") {
    return builtin_history(args);
  }

  return -1; // not a builtin
}

int builtin_cd(std::string_view path) {
  std::string resolved_path{path};

  // Handle home directory expansion
  if (path == "~") {
    const char* home = std::getenv("HOME");
    if (home) {
      resolved_path = home;
    }
  } else if (path.size() >= 2 && path.substr(0, 2) == "~/") {
    const char* home = std::getenv("HOME");
    if (home) {
      // replace "~" with the actual home path
      resolved_path = std::string(home) + std::string(path.substr(1));
    }
  }

  // Check if the directory exists and is actually a directory
  if (fs::exists(resolved_path) && fs::is_directory(resolved_path)) {
    try {
      fs::current_path(resolved_path);
      return EXIT_SUCCESS_CODE;
    } catch (const fs::filesystem_error& e) {
      print_error("cd", std::string(path) + ": " + e.what());
      return EXIT_FAILURE_CODE;
    }
  } else {
    print_error("cd", std::string(path) + ": No such file or directory");
    return EXIT_FAILURE_CODE;
  }
}

int builtin_pwd() {
  try {
    std::cout << fs::current_path().string() << std::endl;
    return EXIT_SUCCESS_CODE;
  } catch (const fs::filesystem_error& e) {
    print_error("pwd", e.what());
    return EXIT_FAILURE_CODE;
  }
}

int builtin_echo(const std::vector<std::string>& args) {
  for (size_t i = 1; i < args.size(); i++) {
    std::cout << args[i];
    if (i < args.size() - 1) {
      std::cout << " ";
    }
  }
  std::cout << std::endl;
  return EXIT_SUCCESS_CODE;
}

int builtin_type(std::string_view arg) {
  if (is_builtin(arg)) {
    std::cout << arg << " is a shell builtin" << std::endl;
  } else {
    std::string path = get_path(arg);
    if (!path.empty()) {
      std::cout << arg << " is " << path << std::endl;
    } else {
      std::cout << arg << ": not found" << std::endl;
      return EXIT_FAILURE_CODE;
    }
  }
  return EXIT_SUCCESS_CODE;
}

int builtin_history(const std::vector<std::string>& args) {
  const auto& command_history = get_command_history();
  
  // Read from file (-r)
  if (args.size() > 1 && args[1] == "-r") {
    if (args.size() < 3) {
      print_error("history", "-r: requires an argument");
      return EXIT_FAILURE_CODE;
    }

    std::string filepath = args[2];
    std::ifstream history_file(filepath);

    if (!history_file.is_open()) {
      print_error("history", filepath + ": No such file or directory");
      return EXIT_FAILURE_CODE;
    }

    std::string line;
    while (std::getline(history_file, line)) {
      if (!line.empty()) {
        add_to_history(line);
      }
    }
    history_file.close();
    return EXIT_SUCCESS_CODE;
  }
  
  // Write to file (-w)
  else if (args.size() > 1 && args[1] == "-w") {
    if (args.size() < 3) {
      print_error("history", "-w: requires an argument");
      return EXIT_FAILURE_CODE;
    }
    std::string filepath = args[2];

    std::ofstream history_file(filepath);

    if (!history_file.is_open()) {
      print_error("history", filepath + ": Cannot write to file");
      return EXIT_FAILURE_CODE;
    }

    for (const auto& cmd : command_history) {
      history_file << cmd << std::endl;
    }
    history_file.close();
    return EXIT_SUCCESS_CODE;
  }
  
  // Append to file (-a)
  else if (args.size() > 1 && args[1] == "-a") {
    if (args.size() < 3) {
      print_error("history", "-a: requires an argument");
      return EXIT_FAILURE_CODE;
    }
    std::string filepath = args[2];

    std::ofstream history_file(filepath, std::ios::app);

    if (!history_file.is_open()) {
      print_error("history", filepath + ": Cannot write to file");
      return EXIT_FAILURE_CODE;
    }

    size_t history_file_index = get_history_file_index();
    for (size_t i = history_file_index; i < command_history.size(); i++) {
      history_file << command_history[i] << std::endl;
    }
    history_file.close();

    // Update the index so we don't write these lines again next time
    set_history_file_index(command_history.size());
    return EXIT_SUCCESS_CODE;
  }

  // Display history
  size_t count = command_history.size(); // default: show all

  // Check if user provided a number (eg: "history 2")
  if (args.size() > 1) {
    try {
      int limit = std::stoi(args[1]);
      if (limit > 0 && (size_t)limit < count) {
        count = (size_t)limit;
      }
    } catch (...) {
      // Ignore invalid arguments
    }
  }

  // Calculate where to start printing
  size_t start_index = command_history.size() - count;
  for (size_t i = start_index; i < command_history.size(); i++) {
    std::cout << "    " << (i + 1) << "  " << command_history[i] << std::endl;
  }
  return EXIT_SUCCESS_CODE;
}

void builtin_exit() noexcept {
  save_history_to_file();
  exit(0);
}

} // namespace shell
