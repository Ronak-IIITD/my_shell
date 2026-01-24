cmake_minimum_required(VERSION 3.13)

project(shell-starter-cpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

# Source files for the main shell executable
file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.hpp)

# Main shell executable
add_executable(shell ${SOURCE_FILES})
target_link_libraries(shell PRIVATE readline)

# Enable testing
enable_testing()

# Create a library with shell components (without main.cpp) for testing
file(GLOB_RECURSE LIB_SOURCE_FILES 
  src/parser.cpp
  src/executor.cpp
  src/builtins.cpp
  src/redirection.cpp
  src/completion.cpp
  src/history.cpp
  src/utils.cpp
)

add_library(shell_lib STATIC ${LIB_SOURCE_FILES})
target_include_directories(shell_lib PUBLIC src)
target_link_libraries(shell_lib PUBLIC readline)

# Test executable
add_executable(shell_tests
  tests/catch_amalgamated.cpp
  tests/test_parser.cpp
  tests/test_builtins.cpp
  tests/test_utils.cpp
  tests/test_redirection.cpp
  tests/test_history.cpp
  tests/test_integration.cpp
)

target_include_directories(shell_tests PRIVATE tests src)
target_link_libraries(shell_tests PRIVATE shell_lib)

# Add test
add_test(NAME ShellTests COMMAND shell_tests)
