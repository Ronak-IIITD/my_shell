#pragma once

#include <readline/readline.h>

namespace shell {

// Initialize readline completion
void init_completion();

// Command generator for readline completion (called repeatedly by readline)
char* command_generator(const char* text, int state);

// Completion hook called by readline when TAB is pressed
char** shell_completion(const char* text, int start, int end);

} // namespace shell
