#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <limits.h>
#include <ostream>
#include <readline/history.h>
#include <readline/readline.h> //readline headers
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
  static size_t list_index;
  static int len;

  if (!state) {
    list_index = 0;
    len = strlen(text);
  }

  while (list_index < builtin_commands.size()) {
    const char *name = builtin_commands[list_index].c_str();
    list_index++;

    // if the builtin_commands name starts with the text user typed
    if (strncmp(name, text, len) == 0) {
      // return a duplicate because readline frees the memory later
      return strdup(name);
    }
  }
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

// int main() {
//   // Flush after every std::cout / std:cerr
//   std::cout << std::unitbuf;
//   std::cerr << std::unitbuf;
//
//   // TODO: Uncomment the code below to pass the first stage
//   std::cout << "$ ";
//   std::string command;
//
//   while (std::getline(std::cin, command)) {
//     if (command.empty()) {
//       std::cout << "$ ";
//       continue;
//     }
//
//     // parse the input using split_line now not by stringstream
//     std::vector<std::string> args = split_line(command);
//
//     // if parsing resulted in empty (eg: just spaces), continue
//     if (args.empty()) {
//       std::cout << "$ ";
//       continue;
//     }
//
//     // redirection logic start
//     RedirectionInfo redirect_file = parse_redirection(args);
//
//     int saved_stdout = -1;
//     int file_fd = -1;
//
//     // check if we found a redirection operator (either > or 2>)
//     if (redirect_file.target_fd != -1) {
//       // 1) save the original stream (stdout or stderr)
//       saved_stdout = dup(redirect_file.target_fd);
//
//       // Decide Flags:
//       // always write only+create
//       // if append is true->O_APPEND. if false -> O_TRUNC (overwrite).
//       int flags = O_WRONLY | O_CREAT;
//       if (redirect_file.append) {
//         flags |= O_APPEND;
//       } else {
//         flags |= O_TRUNC;
//       }
//
//       // open the file (write only, create if missing, truncate/overwrite)
//       // 0644 gives read/write permission to user. read to others
//       file_fd = open(redirect_file.filename.c_str(), flags, 0644);
//
//       if (file_fd < 0) {
//         std::cerr << "bash: " << redirect_file.filename
//                   << ": No such file or directory" << std::endl;
//         // fix - Print prompt before restarting loop
//         std::cout << "$ ";
//         // If we dup'd but failed open, close the dup
//         close(saved_stdout);
//         continue;
//       }
//       // replace standard output (1) with out file
//       dup2(file_fd, redirect_file.target_fd);
//       close(file_fd); // we dont need the raw file descriptor anymore
//     }
//     // redirection logic end
//
//     // normal command execution (standard output is now pointing to the file)
//
//     std::string command_name = args[0];
//
//     if (command_name == "exit") {
//       return 0;
//     } else if (command_name == "echo") {
//       // print arguments separated by space
//       for (size_t i = 1; i < args.size(); i++) {
//         std::cout << args[i];
//         if (i < args.size() - 1) {
//           std::cout << " ";
//         }
//       }
//       std::cout << std::endl;
//     } else if (command_name == "type") {
//       if (args.size() < 2) {
//         // just 'type' with no args, do nothing or print usage
//       } else {
//         std::string arg = args[1];
//         if (arg == "echo" || arg == "exit" || arg == "type" || arg == "pwd"
//         ||
//             arg == "cd") {
//           std::cout << arg << " is a shell builtin" << std::endl;
//         } else {
//           std::string path = get_path(arg);
//           if (!path.empty()) {
//             std::cout << arg << " is " << path << std::endl;
//           } else {
//             std::cout << arg << ": not found" << std::endl;
//           }
//         }
//       }
//     } else if (command_name == "pwd") {
//       // manual function calling
//
//       // std::string cwd=get_pwd();
//
//       // if (!cwd.empty()) {
//       //   std::cout<<cwd<<std::endl;
//       // }
//       // We do NOT write 'return 0' here.
//       // We let the loop finish so the user can type the next command.
//
//       // directly come from filesystem :)
//       std::cout << fs::current_path().string() << std::endl;
//     } else if (command_name == "cd") {
//       if (args.size() >= 2) {
//         builtin_cd(args[1]);
//       } else {
//         // 'cd' with no arguments usually goes home, but for test we can
//         ignore
//         // or go home
//         builtin_cd("~");
//       }
//     } else {
//
//       // ---- RUNNING EXTERNAL PROGRAMS ----
//       std::string path = get_path(command_name);
//
//       if (path.empty()) {
//         std::cout << command_name << ": command not found" << std::endl;
//       } else {
//         // execute_external takes the vector, which is already correctly
//         parsed!
//         // it will clean arguments (without quotes) to the program.
//         execute_external(path, args);
//       }
//     }
//
//     // restore
//     if (saved_stdout != -1) {
//       // restore the specific FD (stdout or stderr)
//       dup2(saved_stdout, redirect_file.target_fd);
//       close(saved_stdout);
//     }
//
//     std::cout << "$ ";
//   }
// }

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
    std::string command_name = args[0];

    if (command_name == "exit") {
      return 0; // Shell exits
    } else if (command_name == "echo") {
      for (size_t i = 1; i < args.size(); i++) {
        std::cout << args[i];
        if (i < args.size() - 1)
          std::cout << " ";
      }
      std::cout << std::endl;
    } else if (command_name == "type") {
      if (args.size() > 1) {
        std::string arg = args[1];
        if (arg == "echo" || arg == "exit" || arg == "type" || arg == "pwd" ||
            arg == "cd")
          std::cout << arg << " is a shell builtin" << std::endl;
        else {
          std::string p = get_path(arg);
          if (!p.empty())
            std::cout << arg << " is " << p << std::endl;
          else
            std::cout << arg << ": not found" << std::endl;
        }
      }
    } else if (command_name == "pwd") {
      std::cout << fs::current_path().string() << std::endl;
    } else if (command_name == "cd") {
      if (args.size() >= 2)
        builtin_cd(args[1]);
      else
        builtin_cd("~");
    } else {
      // External Program
      std::string path = get_path(command_name);
      if (path.empty()) {
        std::cout << command_name << ": command not found" << std::endl;
      } else {
        execute_external(path, args);
      }
    }

    // 6. RESTORE OUTPUT
    if (saved_fd != -1) {
      dup2(saved_fd, redir.target_fd);
      close(saved_fd);
    }
  }

  return 0;
}
