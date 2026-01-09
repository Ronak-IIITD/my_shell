#include <cstddef>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

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
          remained_command == "type") {
        std::cout << remained_command << " is a shell builtin" << std::endl;
      } else {
        // You usually also need to handle cases where it's NOT found
        // for the "type" command
        std::cout << remained_command << ": not found" << std::endl;
      }
    } else {
      // this only runs if the command was not as what we get in input is not
      // a valid command
      std::cout << command << ": command not found" << std::endl;
    }

    std::cout << "$ ";
  }
}
