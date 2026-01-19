#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <ios>
#include <iostream>
#include <limits.h>
#include <new>
#include <ostream>
#include <readline/chardefs.h>
#include <readline/history.h>
#include <readline/readline.h> //readline headers
#include <set>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h> //required for wait
#include <system_error>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

struct RedirectionInfo {
  std::string filename;
  int target_fd; // 1 for stdout, 2 for stderr, -1 for none
  bool append;   // true=append (>>), false=overwrite (>)
};

std::vector<std::string> builtin_commands = {"echo", "exit", "type", "pwd",
                                             "cd"};

std::string get_path(std::string command) {
  const char *get_env_cstr = std::getenv("PATH");
  if (get_env_cstr == nullptr) {
    return ""; // PATH not set!, can't find command
  }
  std::string path_env = get_env_cstr;
  std::stringstream ss(path_env);
  std::string path_dir;

  while (std::getline(ss, path_dir, ':')) {
    fs::path full_path = path_dir;
    full_path /= command; // append command to directory (e.g., /usr/bin/ + ls)

    // Check if file exists and is executable
    if (fs::exists(full_path) && access(full_path.c_str(), X_OK) == 0) {
      return full_path.string();
    }
  }
  return "";
}

void execute_external(const std::string &path,
                      const std::vector<std::string> &parts) {
  // 1) convert std::vector<std::string> to char* array (c-style)
  std::vector<char *> c_args;
  for (const auto &part : parts) {
    c_args.push_back(const_cast<char *>(part.c_str()));
  }
  c_args.push_back(nullptr); // null terminates the list otherwise it will give
                             // segmentation fault error

  // fork and execute
  pid_t pid = fork();

  if (pid == 0) {
    // child process
    execv(path.c_str(), c_args.data());
    exit(1); // should only reach here if execv fails
  } else if (pid > 0) {
    // parent process
    wait(nullptr);
  } else {
    std::cerr << "Fork Failed" << std::endl;
  }
}

// Source - https://stackoverflow.com/a/69847310
// Posted by Remy Lebeau, modified by community. See post 'Timeline' for change
// history Retrieved 2026-01-11, License - CC BY-SA 4.0

// --builtin-cd --
// this function handles the logic to change the directory

void builtin_cd(std::string path) {

  // for checking home directory
  if (path == "~") {
    const char *home = std::getenv("HOME");
    if (home) {
      path = home;
    }
  } else if (path.substr(0, 2) == "~/") {
    const char *home = std::getenv("HOME");
    if (home) {
      // replace "~" with the actual home path
      path = std::string(home) + path.substr(1);
    }
  }
  // check if the directory exists and is actually a directory
  if (fs::exists(path) && fs::is_directory(path)) {
    // this tells the operating systeme to change the directory
    // equivalent to the c function chdir()
    try {
      fs::current_path(path);
    } catch (const fs::filesystem_error &e) {
      // should permission fail, etc.
      std::cerr << "cd: " << path << ": " << e.what() << std::endl;
    }
  } else {
    std::cout << "cd: " << path << ": No such file or directory" << std::endl;
  }
}

// --- PARSER: Handle Single Quotes ---
std::vector<std::string> split_line(const std::string &input) {
  std::vector<std::string> args;
  std::string current_tokens;
  bool in_single_quotes = false; // are we currently inside '...'?
  bool in_double_quotes = false; // are we currently inside "..."?

  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];

    // 1) Handle Backslashes (\)
    // we handle Backslashes first because they can escape quotes too (eg: \")
    if (c == '\\') {
      // case A: inside single quotes ('....')
      // Backslashes are not special inside the single quotes. they are just
      // text
      if (in_single_quotes) {
        current_tokens += c;
      }
      // case B: inside the double quotes ("....")
      // Backslashes only escape specific characters: \, ", $, and newline
      else if (in_double_quotes) {
        if (i + 1 < input.length()) {
          char next_c = input[i + 1];
          // if the next char is one of the special ones, escape it
          if (next_c == '\\' || next_c == '"' || next_c == '$' ||
              next_c == '\n') {
            current_tokens += next_c;
            i++; // skip the next character (we just consumed it)
          } else {
            // otherwise, keep the Backslashe literal (eg: "\a" -> "\a")
            current_tokens += c;
          }
        } else {
          current_tokens += c; // trailing Backslashes at the end of string
        }
      }
      // case C: outside quotes
      // the Backslashes escapes any character that follows in
      else {
        if (i + 1 < input.length()) {
          current_tokens += input[i + 1]; // add the next char literally
          i++; // skip the next character (we just consumed it)
        }
      }
      continue;
    }
    // 2) handle quotes and spaces
    if (c == '\'' && !in_double_quotes) {
      in_single_quotes = !in_single_quotes;
    } else if (c == '"' && !in_single_quotes) {
      in_double_quotes = !in_double_quotes;
    } else if (c == ' ' && !in_single_quotes && !in_double_quotes) {
      if (!current_tokens.empty()) {
        args.push_back(current_tokens);
        current_tokens.clear();
      }
    } else {
      current_tokens += c;
    }
  }
  if (!current_tokens.empty()) {
    args.push_back(current_tokens);
  }
  return args;
}

RedirectionInfo parse_redirection(std::vector<std::string> &args) {

  RedirectionInfo info = {"", -1, false}; // default: no redirection

  for (size_t i = 0; i < args.size(); i++) {
    // ">" operator to redirect standard output to a file
    if (args[i] == ">" || args[i] == "1>") {
      if (i + 1 < args.size()) {
        info.filename = args[i + 1];
        info.target_fd = 1; // 1
        info.append = false;
        args.erase(args.begin() + i, args.begin() + i + 2);
        return info;
      }
    } else if (args[i] == ">>" || args[i] == "1>>") {
      if (i + 1 < args.size()) {
        info.filename = args[i + 1];
        info.target_fd = 1; // always standard output (1)
        info.append = true; // enable append mode
        args.erase(args.begin() + i, args.begin() + i + 2);
        return info;
      }
    } // standard error overwrite (2>)
    else if (args[i] == "2>") {
      if (i + 1 < args.size()) {
        info.filename = args[i + 1];
        info.target_fd = 2; // 2
        info.append = false;
        args.erase(args.begin() + i, args.begin() + i + 2);
        return info;
      }
    } // standard error append (2>>)
    else if (args[i] == "2>>") {
      if (i + 1 < args.size()) {
        info.filename = args[i + 1];
        info.target_fd = 2;
        info.append = true;
        args.erase(args.begin() + i, args.begin() + i + 2);
        return info;
      }
    }
  }
  return info;
}

// AUTOCOMPLETE LOGIC START

// 1) The Generator
// Readline calls this repeatedly. state is 0 the first time, then 1, 2, etc.
// We return the next match, or nullptr when done.

char *command_generator(const char *text, int state) {
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
    for (const auto &cmd : builtin_commands) {
      // check if builtin starts with 'text'
      if (cmd.find(text_str) == 0) {
        matches.push_back(cmd);
        found.insert(cmd);
      }
    }
    // 2) search for path executable
    const char *path_env = std::getenv("PATH");
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
          for (const auto &entry : fs::directory_iterator(p)) {
            // skip if not a regular file (eg: skip directories/ socekts inside
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

// 2) The Hook
// This function tells readline: "if the user hits TAB at the start of the line,
// use our custom Generator."
char **shell_completion(const char *text, int start, int end) {
  // only AUTOCOMPLETE the first word (the command)
  if (start == 0) {
    // prevent readline from falling back to filename completion
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, command_generator);
  }
  return nullptr;
}

// AUTOCOMPLETE LOGIC END

// manual implementation to get pwd without current_path() :)

// std::string get_pwd() {
// PATH_MAX is usually defined in limits.h (often 4096 bytes)
// If not defined, we fallback to 4096.
// #ifndef PATH_MAX
// #define PATH_MAX 4096
// #endif

// char buffer[PATH_MAX];

// getcwd(buffer, size);
// Returns a pointer to buffer on success, or NULL on failure.
// if (getcwd(buffer, sizeof(buffer)) != nullptr){
//   return std::string(buffer);
// } else {
// If it fails (e.g., path is too long), print a system error
//     perror("pwd error");
//     return "";
//   }
// }

// this function runs builtins OR external commands
// it returns true if it ran a builtin, false if it tried to run external (execv
// usually doesn't return).

bool run_command_logic(const std::vector<std::string> &args) {
  std::string command_name = args[0];

  if (command_name == "exit") {
    // in a pipline, this exits the child (correct)
    // in main, we handle exit separately to close the shell
    exit(0);
  } else if (command_name == "echo") {
    for (size_t i = 1; i < args.size(); i++) {
      std::cout << args[i];
      if (i < args.size() - 1)
        std::cout << " ";
    }
    std::cout << std::endl;
    return true;
  } else if (command_name == "type") {
    if (args.size() > 1) {
      std::string arg = args[1];
      if (arg == "echo" || arg == "exit" || arg == "type" || arg == "pwd" ||
          arg == "cd" || arg=="history")
        std::cout << arg << " is a shell builtin" << std::endl;
      else {
        std::string p = get_path(arg);
        if (!p.empty())
          std::cout << arg << " is " << p << std::endl;
        else
          std::cout << arg << ": not found" << std::endl;
      }
    }
    return true;
  } else if (command_name == "pwd") {
    std::cout << fs::current_path().string() << std::endl;
    return true;
  } else if (command_name == "cd") {
    if (args.size() >= 2)
      builtin_cd(args[1]);
    else
      builtin_cd("~");
    return true;
  } else {
    // external commands
    std::string path = get_path(command_name);
    if (path.empty()) {
      std::cout << command_name << ": command not found" << std::endl;
      // return true here so the caller knows we "handled" the error
      // and doesn't crash, but usually for pipeline we exit(1).
      return true;
    } else {
      execute_external(path, args); // this contains fork/execv for normal cases
      return false;
    }
  }
  return false;
}

std::vector<std::vector<std::string>>
parse_pipeline_args(const std::vector<std::string> &args) {
  std::vector<std::vector<std::string>> commands;
  std::vector<std::string> current_cmd;

  for (const auto &arg : args) {
    if (arg == "|") {
      if (!current_cmd.empty()) {
        commands.push_back(current_cmd);
        current_cmd.clear();
      }
    } else {
      current_cmd.push_back(arg);
    }
  }
  if (!current_cmd.empty()) {
    commands.push_back(current_cmd);
  }
  return commands;
}

void execute_n_pipeline(const std::vector<std::vector<std::string>> &commands) {
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
      const std::vector<std::string> &cmd_args = commands[i];
      std::string cmd = cmd_args[0];

      // Check for Builtins
      // We use run_command_logic, but we must ensure we EXIT afterwards.
      bool is_builtin = (cmd == "echo" || cmd == "type" || cmd == "pwd" ||
                         cmd == "cd" || cmd == "exit");

      if (is_builtin) {
        run_command_logic(cmd_args);
        exit(0); // Important: Child must exit after builtin finishes
      } else {
        // External Command
        std::string path = get_path(cmd);
        if (path.empty()) {
          std::cerr << cmd << ": command not found" << std::endl;
          exit(1);
        }

        // Convert to C-style args
        std::vector<char *> c_args;
        for (const auto &arg : cmd_args)
          c_args.push_back(const_cast<char *>(arg.c_str()));
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

int main() {
  // Use unitbuf to ensure output is flushed immediately
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // 1. REGISTER AUTOCOMPLETE
  // Tell readline to use our custom function when TAB is pressed
  rl_attempted_completion_function = shell_completion;

  char *input_cstr;

  // 2. MAIN LOOP
  // readline("$ ") prints the prompt and handles user input (including
  // Tab/Arrow keys) It returns nullptr when the user presses Ctrl+D (EOF)
  while ((input_cstr = readline("$ ")) != nullptr) {

    std::string input_line(input_cstr);

    // Add valid commands to history so Up-Arrow works
    if (!input_line.empty()) {
      add_history(input_cstr);
    }

    // Readline allocates memory for the input string, we must free it
    free(input_cstr);

    // Skip empty lines
    if (input_line.empty()) {
      continue;
    }

    // 3. PARSE ARGS
    std::vector<std::string> args = split_line(input_line);
    if (args.empty())
      continue;

    // --CHECK FOR PIPELINE--
    // use the new parser to verify if we have multiple commands separated by
    // "|"
    std::vector<std::vector<std::string>> commands = parse_pipeline_args(args);

    if (commands.size() > 1) {
      // we have pipeline (eg: cmd1 | cmd2 ....)
      execute_n_pipeline(commands);
      continue; // skip the rest of the loop
    }
    // --END PIPELINE CHECK--

    // 4. HANDLE REDIRECTION
    RedirectionInfo redir = parse_redirection(args);

    int saved_fd = -1;
    int file_fd = -1;

    if (redir.target_fd != -1) {
      // Save original stream
      saved_fd = dup(redir.target_fd);

      // Determine flags: Write Only + Create + (Append OR Truncate)
      int flags = O_WRONLY | O_CREAT;
      if (redir.append) {
        flags |= O_APPEND;
      } else {
        flags |= O_TRUNC;
      }

      // Open file (0644 = rw-r--r--)
      file_fd = open(redir.filename.c_str(), flags, 0644);

      if (file_fd < 0) {
        std::cerr << "bash: " << redir.filename << ": No such file or directory"
                  << std::endl;
        // No need to print "$ ", readline will do it next loop
        close(saved_fd);
        continue;
      }

      // Apply Redirection
      dup2(file_fd, redir.target_fd);
      close(file_fd);
    }

    // 5. EXECUTE COMMANDS
    // use our new centralized logic for main as well!
    // but check for "exit" explicitly first to break the loop cleanly.
    if (args[0] == "exit") {
      return 0;
    }

    // run command (pwd, cd, echo, type, or external)
    run_command_logic(args);

    // 6. RESTORE OUTPUT
    if (saved_fd != -1) {
      dup2(saved_fd, redir.target_fd);
      close(saved_fd);
    }
  }

  return 0;
}
