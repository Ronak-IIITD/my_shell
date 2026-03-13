cmake_minimum_required(VERSION 3.13)

project(shell-starter-cpp)

# Prefer sources in src/, but support root-level main.cpp for repos that
# migrated away from the starter layout.
file(GLOB_RECURSE SOURCE_FILES src/*.cpp)
if(NOT SOURCE_FILES)
	set(SOURCE_FILES main.cpp)
endif()

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

add_executable(shell ${SOURCE_FILES})

target_link_libraries(shell PRIVATE readline)
