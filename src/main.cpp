#include "builtins.h"
#include "completion.h"
#include "executor.h"
#include "history.h"
#include "parser.h"
#include "redirection.h"
#include <cstdlib>
#include <iostream>
#include <readline/history.h>
#include <readline/readline.h>
#include <string>
#include <vector>

int main() {
  // Use unitbuf to ensure output is flushed immediately
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // Initialize history management
  shell::init_history();
  shell::load_history_from_file();

  // Initialize readline completion
  shell::init_completion();

  char* input_cstr;

  // Main REPL loop
  // readline("$ ") prints the prompt and handles user input (including
  // Tab/Arrow keys) It returns nullptr when the user presses Ctrl+D (EOF)
  while ((input_cstr = readline("$ ")) != nullptr) {
    std::string input_line(input_cstr);

    // Add valid commands to history so Up-Arrow works
    if (!input_line.empty()) {
      add_history(input_cstr);
      shell::add_to_history(input_line);
    }

    // Readline allocates memory for the input string, we must free it
    free(input_cstr);

    // Skip empty lines
    if (input_line.empty()) {
      continue;
    }

    // Parse input into tokens
    std::vector<std::string> args = shell::split_line(input_line);
    if (args.empty()) {
      continue;
    }

    // Check for exit command before doing anything else
    if (args[0] == "exit") {
      shell::save_history_to_file();
      return 0;
    }

    // Check for pipeline (multiple commands separated by |)
    std::vector<std::vector<std::string>> commands =
        shell::parse_pipeline_args(args);

    if (commands.size() > 1) {
      // Execute pipeline
      shell::execute_pipeline(commands);
      continue;
    }

    // Handle redirection
    shell::RedirectionInfo redir = shell::parse_redirection(args);
    int saved_fd = shell::apply_redirection(redir);

    // If redirection failed, skip execution
    if (redir.target_fd != -1 && saved_fd == -1) {
      continue;
    }

    // Execute single command (builtin or external)
    shell::execute_command(args);

    // Restore output if redirected
    shell::restore_redirection(saved_fd, redir.target_fd);
  }

  // Save history on exit (Ctrl+D)
  shell::save_history_to_file();

  return 0;
}
