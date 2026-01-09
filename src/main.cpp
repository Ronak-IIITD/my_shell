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
    } else {
      // this only runs if the command was not as what we get in input is not
      // valid command
      std::cout << command << ": command not found" << std::endl;
    }

    std::cout << "$ ";

    //   if (command == "exit") {
    //     return 0;
    //   }

    //   std::stringstream ss(command);
    //   std::string word;
    //   while (ss >> word) {
    //     if (word == "echo") {
    //       std::string remaining;
    //       std::getline(ss, remaining);

    //       if (!remaining.empty() && remaining[0] == ' ') {
    //         remaining = remaining.substr(1);
    //       }
    //       std::cout << remaining << std::endl;
    //       break;
    //     }
    //   }

    //   std::cout << command << ": command not found" << std::endl;
    //   std::cout << "$ ";
    // }
  }
}
