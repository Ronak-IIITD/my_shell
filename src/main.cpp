#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h> //required for wait
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

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

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  std::cout << "$ ";
  std::string command;

  while (std::getline(std::cin, command)) {

    if (command.empty()) {
      std::cout << "$ ";
      continue;
    }

    std::stringstream ss(command);
    std::string command_name;

    // extract the first word
    ss >> command_name;

    if (command_name == "exit") {
      return 0;
    } else if (command_name == "echo") {
      std::string remaining;
      // get the rest of the line
      std::getline(ss, remaining);

      // Trim the leading space that getline picks up
      // (operator>> skips whitespace, but getline reads from the very next
      // character)
      if (!remaining.empty() && remaining[0] == ' ') {
        remaining = remaining.substr(1);
      }
      std::cout << remaining << std::endl;
    } else if (command_name == "type") {
      std::string remained_command;
      // Use >> to skip the whitespace and get the next word cleanly
      ss >> remained_command;

      if (remained_command == "echo" || remained_command == "exit" ||
          remained_command == "type" || remained_command == "pwd") {
        std::cout << remained_command << " is a shell builtin" << std::endl;
      } else {
        // You usually also need to handle cases where it's NOT found
        // for the "type" command
        std::string path = get_path(remained_command);
        if (!path.empty()) {
          std::cout << remained_command << " is " << path << std::endl;
        } else {
          std::cout << remained_command << ": not found" << std::endl;
        }
      }
    } else if (command_name == "pwd") {
      // manual function calling

      // std::string cwd=get_pwd();

      // if (!cwd.empty()) {
      //   std::cout<<cwd<<std::endl;
      // }
      // We do NOT write 'return 0' here.
      // We let the loop finish so the user can type the next command.

      // directly come from filesystem :)
      std::cout << fs::current_path().string() << std::endl;
    } else {

      // ---- RUNNING EXTERNAL PROGRAMS ----
      std::string path = get_path(command_name);

      if (path.empty()) {
        std::cout << command_name << ": command not found" << std::endl;
      } else {
        // collect all arguments including the command name itself
        std::vector<std::string> args;
        args.push_back(command_name);

        std::string arg;
        while (ss >> arg) {
          args.push_back(arg);
        }

        // just one clearn function call
        execute_external(path, args);
      }
    }

    std::cout << "$ ";
  }
}
