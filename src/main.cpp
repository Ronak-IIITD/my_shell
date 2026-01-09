#include <cstddef>
#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  std::cout << "$ ";
  std::string command;

  while (std::getline(std::cin, command)) {

    if (command == "exit") {
      return 0;
    }

    std::cout << command << ": command not found" << std::endl;
    std::cout << "$ ";
  }
}
