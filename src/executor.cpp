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
    exit(1); // Should only reach here if execv fails
  } else if (pid > 0) {
    // Parent process
    wait(nullptr);
  } else {
    std::cerr << "Fork Failed" << std::endl;
  }
}

bool execute_command(const std::vector<std::string>& args) {
  if (args.empty()) {
    return false;
  }

  const std::string& command_name = args[0];

  // Check if it's a builtin
  if (is_builtin(command_name)) {
    run_builtin(args);
    return true;
  }

  // External command
  std::string path = get_path(command_name);
  if (path.empty()) {
    std::cout << command_name << ": command not found" << std::endl;
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
        perror("pipe");
        return;
      }
    }

    pid_t pid = fork();
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
        run_builtin(cmd_args);
        exit(0); // Important: Child must exit after builtin finishes
      } else {
        // External Command
        std::string path = get_path(cmd);
        if (path.empty()) {
          std::cerr << cmd << ": command not found" << std::endl;
          exit(1);
        }

        // Convert to C-style args
        std::vector<char*> c_args;
        for (const auto& arg : cmd_args) {
          c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        execv(path.c_str(), c_args.data());
        exit(1); // Exec failed
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
    waitpid(p, nullptr, 0);
  }
}

} // namespace shell
