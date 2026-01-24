#include "executor.h"
#include "builtins.h"
#include "utils.h"
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace shell {

void execute_external(const std::string& path,
                      const std::vector<std::string>& parts) {
  // Convert std::vector<std::string> to char* array (c-style)
  std::vector<char*> c_args;
  for (const auto& part : parts) {
    c_args.push_back(const_cast<char*>(part.c_str()));
  }
  c_args.push_back(nullptr); // null terminates the list

  // Fork and execute
  pid_t pid = fork();

  if (pid == 0) {
    // Child process
    execv(path.c_str(), c_args.data());
    // If execv returns, it failed
    print_error(parts[0], "execution failed");
    exit(EXIT_COMMAND_NOT_FOUND);
  } else if (pid > 0) {
    // Parent process - wait for child
    int status;
    waitpid(pid, &status, 0);
    // Could check WIFEXITED(status) and WEXITSTATUS(status) here
  } else {
    // Fork failed
    print_error("fork", "failed to create child process");
  }
}

bool execute_command(const std::vector<std::string>& args) {
  if (args.empty()) {
    return false;
  }

  const std::string& command_name = args[0];

  // Check if it's a builtin
  if (is_builtin(command_name)) {
    int exit_code = run_builtin(args);
    // For now, we don't do anything with the exit code in interactive mode
    // but it's available for future use (e.g., scripting mode with set -e)
    (void)exit_code;
    return true;
  }

  // External command
  std::string path = get_path(command_name);
  if (path.empty()) {
    print_error(command_name, "command not found");
    return true; // handled the error
  }

  execute_external(path, args);
  return false;
}

void execute_pipeline(const std::vector<std::vector<std::string>>& commands) {
  int num_cmds = commands.size();
  int prev_pipe_fd = -1;   // Holds the Read End of the previous pipe
  std::vector<pid_t> pids; // Keep track of children to wait for them later

  for (int i = 0; i < num_cmds; i++) {
    int pipefd[2];
    bool is_last = (i == num_cmds - 1);

    // Create a pipe for the *next* connection (unless we are the last command)
    if (!is_last) {
      if (pipe(pipefd) == -1) {
        print_error("pipe", "failed to create pipe");
        // Clean up any existing children
        for (pid_t p : pids) {
          waitpid(p, nullptr, 0);
        }
        return;
      }
    }

    pid_t pid = fork();
    
    if (pid == -1) {
      // Fork failed
      print_error("fork", "failed to create child process");
      if (!is_last) {
        close(pipefd[0]);
        close(pipefd[1]);
      }
      // Clean up existing children
      for (pid_t p : pids) {
        waitpid(p, nullptr, 0);
      }
      return;
    }
    
    if (pid == 0) {
      // --- CHILD PROCESS ---

      // 1. INPUT SETUP: If there was a previous pipe, read from it
      if (prev_pipe_fd != -1) {
        dup2(prev_pipe_fd, STDIN_FILENO);
        close(prev_pipe_fd);
      }

      // 2. OUTPUT SETUP: If not the last command, write to the new pipe
      if (!is_last) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        close(pipefd[0]); // Child doesn't read from its own output pipe
      }

      // 3. EXECUTE COMMAND
      const std::vector<std::string>& cmd_args = commands[i];
      std::string cmd = cmd_args[0];

      // Check for Builtins
      if (is_builtin(cmd)) {
        int exit_code = run_builtin(cmd_args);
        exit(exit_code); // Important: Child must exit after builtin finishes
      } else {
        // External Command
        std::string path = get_path(cmd);
        if (path.empty()) {
          print_error(cmd, "command not found");
          exit(EXIT_COMMAND_NOT_FOUND);
        }

        // Convert to C-style args
        std::vector<char*> c_args;
        for (const auto& arg : cmd_args) {
          c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        execv(path.c_str(), c_args.data());
        // If we reach here, execv failed
        print_error(cmd, "execution failed");
        exit(EXIT_COMMAND_NOT_FOUND);
      }
    }

    // --- PARENT PROCESS ---
    pids.push_back(pid);

    // 1. Cleanup used input pipe (child has it now, we don't need it)
    if (prev_pipe_fd != -1) {
      close(prev_pipe_fd);
    }

    // 2. Setup input for the NEXT loop iteration
    if (!is_last) {
      prev_pipe_fd = pipefd[0]; // Save the read end
      close(pipefd[1]);         // Close write end (child has it)
    }
  }

  // Wait for ALL children to finish
  for (pid_t p : pids) {
    int status;
    waitpid(p, &status, 0);
    // Could track exit statuses here if needed
  }
}

} // namespace shell
