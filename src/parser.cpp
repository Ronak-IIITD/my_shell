#include "parser.h"

namespace shell {

// --- PARSER: Handle Single Quotes, Double Quotes, and Escapes ---
std::vector<std::string> split_line(const std::string& input) {
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
      // Backslashes are not special inside the single quotes. they are just text
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

std::vector<std::vector<std::string>>
parse_pipeline_args(const std::vector<std::string>& args) {
  std::vector<std::vector<std::string>> commands;
  std::vector<std::string> current_cmd;

  for (const auto& arg : args) {
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

} // namespace shell
