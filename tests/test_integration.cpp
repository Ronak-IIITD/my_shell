#include "catch_amalgamated.hpp"
#include "parser.h"
#include "builtins.h"
#include "redirection.h"
#include "utils.h"
#include <sstream>
#include <iostream>

using namespace shell;

// Helper to capture stdout
class StdoutCapture {
public:
    StdoutCapture() {
        old_buf = std::cout.rdbuf();
        std::cout.rdbuf(buffer.rdbuf());
    }
    
    ~StdoutCapture() {
        std::cout.rdbuf(old_buf);
    }
    
    std::string get() {
        return buffer.str();
    }
    
private:
    std::stringstream buffer;
    std::streambuf* old_buf;
};

TEST_CASE("Integration - Parse and execute builtin", "[integration]") {
    SECTION("Echo with multiple arguments") {
        auto tokens = split_line("echo hello world test");
        
        REQUIRE(tokens.size() == 4);
        REQUIRE(is_builtin(tokens[0]));
        
        StdoutCapture capture;
        int result = run_builtin(tokens);
        std::string output = capture.get();
        
        REQUIRE(result == EXIT_SUCCESS_CODE);
        REQUIRE(output.find("hello world test") != std::string::npos);
    }
    
    SECTION("Echo with quotes") {
        auto tokens = split_line("echo \"hello world\"");
        
        REQUIRE(tokens.size() == 2);
        REQUIRE(tokens[1] == "hello world");
        
        StdoutCapture capture;
        int result = run_builtin(tokens);
        std::string output = capture.get();
        
        REQUIRE(result == EXIT_SUCCESS_CODE);
        REQUIRE(output.find("hello world") != std::string::npos);
    }
    
    SECTION("Type command") {
        auto tokens = split_line("type echo");
        
        REQUIRE(tokens.size() == 2);
        
        StdoutCapture capture;
        int result = run_builtin(tokens);
        std::string output = capture.get();
        
        REQUIRE(result == EXIT_SUCCESS_CODE);
        REQUIRE(output.find("builtin") != std::string::npos);
    }
}

TEST_CASE("Integration - Complex quote parsing", "[integration]") {
    SECTION("Mixed quotes and escapes") {
        auto tokens = split_line("echo \"hello 'world'\" test");
        
        REQUIRE(tokens.size() == 3);
        REQUIRE(tokens[0] == "echo");
        REQUIRE(tokens[1] == "hello 'world'");
        REQUIRE(tokens[2] == "test");
    }
    
    SECTION("Escaped spaces in path") {
        auto tokens = split_line("cd /path\\ with\\ spaces");
        
        REQUIRE(tokens.size() == 2);
        REQUIRE(tokens[1] == "/path with spaces");
    }
    
    SECTION("Complex command with many quote types") {
        auto tokens = split_line("echo 'single' \"double\" unquoted 'mix'\"ed\"");
        
        REQUIRE(tokens.size() == 5);
        REQUIRE(tokens[1] == "single");
        REQUIRE(tokens[2] == "double");
        REQUIRE(tokens[3] == "unquoted");
        REQUIRE(tokens[4] == "mixed");
    }
}

TEST_CASE("Integration - Redirection parsing and execution", "[integration]") {
    SECTION("Parse command with redirection") {
        auto tokens = split_line("echo hello > output.txt");
        
        REQUIRE(tokens.size() == 4); // echo, hello, >, output.txt
        
        // After parsing redirection
        auto redir = parse_redirection(tokens);
        
        REQUIRE(redir.target_fd == 1);
        REQUIRE(redir.filename == "output.txt");
        REQUIRE(tokens.size() == 2); // echo, hello (redirection removed)
    }
    
    SECTION("Parse stderr redirection") {
        auto tokens = split_line("command 2> error.log");
        
        auto redir = parse_redirection(tokens);
        
        REQUIRE(redir.target_fd == 2);
        REQUIRE(redir.filename == "error.log");
    }
    
    SECTION("Quoted filename in redirection") {
        auto tokens = split_line("echo test > \"my file.txt\"");
        
        REQUIRE(tokens.size() == 4);
        REQUIRE(tokens[3] == "my file.txt");
        
        auto redir = parse_redirection(tokens);
        REQUIRE(redir.filename == "my file.txt");
    }
}

TEST_CASE("Integration - Pipeline parsing", "[integration]") {
    SECTION("Simple pipeline structure") {
        auto tokens = split_line("ls -la | grep cpp");
        auto pipeline = parse_pipeline_args(tokens);
        
        REQUIRE(pipeline.size() == 2);
        REQUIRE(pipeline[0].size() == 2);
        REQUIRE(pipeline[0][0] == "ls");
        REQUIRE(pipeline[0][1] == "-la");
        REQUIRE(pipeline[1].size() == 2);
        REQUIRE(pipeline[1][0] == "grep");
        REQUIRE(pipeline[1][1] == "cpp");
    }
    
    SECTION("Three-stage pipeline") {
        auto tokens = split_line("cat file.txt | grep error | wc -l");
        auto pipeline = parse_pipeline_args(tokens);
        
        REQUIRE(pipeline.size() == 3);
        REQUIRE(pipeline[0][0] == "cat");
        REQUIRE(pipeline[1][0] == "grep");
        REQUIRE(pipeline[2][0] == "wc");
    }
    
    SECTION("Pipeline with quoted arguments") {
        auto tokens = split_line("echo \"hello world\" | grep hello");
        auto pipeline = parse_pipeline_args(tokens);
        
        REQUIRE(pipeline.size() == 2);
        REQUIRE(pipeline[0].size() == 2);
        REQUIRE(pipeline[0][1] == "hello world");
    }
}

TEST_CASE("Integration - PATH lookup and type checking", "[integration]") {
    SECTION("Find external command") {
        std::string ls_path = get_path("ls");
        REQUIRE_FALSE(ls_path.empty());
        
        // Type should find it
        int result = builtin_type("ls");
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }
    
    SECTION("Builtin vs external") {
        REQUIRE(is_builtin("echo"));
        REQUIRE_FALSE(is_builtin("ls"));
        
        std::string echo_path = get_path("echo");
        // May or may not find external echo
        
        std::string ls_path = get_path("ls");
        REQUIRE_FALSE(ls_path.empty());
    }
}

TEST_CASE("Integration - Real-world command scenarios", "[integration]") {
    SECTION("Git commit command") {
        auto tokens = split_line("git commit -m \"Initial commit with spaces\"");
        
        REQUIRE(tokens.size() == 4);
        REQUIRE(tokens[0] == "git");
        REQUIRE(tokens[1] == "commit");
        REQUIRE(tokens[2] == "-m");
        REQUIRE(tokens[3] == "Initial commit with spaces");
    }
    
    SECTION("Find command with pattern") {
        auto tokens = split_line("find /home/user -name '*.cpp' -type f");
        
        REQUIRE(tokens.size() == 6);
        REQUIRE(tokens[3] == "*.cpp");
    }
    
    SECTION("Grep with regex") {
        auto tokens = split_line("grep -r \"error.*failed\" /var/log");
        
        REQUIRE(tokens.size() == 4);
        REQUIRE(tokens[2] == "error.*failed");
    }
    
    SECTION("Complex pipeline") {
        auto tokens = split_line("ps aux | grep python | grep -v grep | awk '{print $2}'");
        auto pipeline = parse_pipeline_args(tokens);
        
        REQUIRE(pipeline.size() == 4);
    }
    
    SECTION("Command with multiple redirections") {
        auto tokens = split_line("command > output.txt 2> error.txt");
        
        // Parse first redirection
        auto redir1 = parse_redirection(tokens);
        REQUIRE(redir1.filename == "output.txt");
        
        // Parse second redirection (from remaining tokens)
        auto redir2 = parse_redirection(tokens);
        REQUIRE(redir2.filename == "error.txt");
    }
}

TEST_CASE("Integration - Edge cases and stress tests", "[integration]") {
    SECTION("Very long command line") {
        std::string long_arg(10000, 'a');
        auto tokens = split_line("echo " + long_arg);
        
        REQUIRE(tokens.size() == 2);
        REQUIRE(tokens[1].length() == 10000);
        
        StdoutCapture capture;
        int result = run_builtin(tokens);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }
    
    SECTION("Many arguments") {
        std::string cmd = "echo";
        for (int i = 0; i < 100; i++) {
            cmd += " arg" + std::to_string(i);
        }
        
        auto tokens = split_line(cmd);
        REQUIRE(tokens.size() == 101);
        
        StdoutCapture capture;
        int result = run_builtin(tokens);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }
    
    SECTION("Deeply nested quotes") {
        auto tokens = split_line("echo \"outer 'inner' outer\"");
        
        REQUIRE(tokens.size() == 2);
        REQUIRE(tokens[1] == "outer 'inner' outer");
    }
    
    SECTION("Empty command") {
        auto tokens = split_line("");
        REQUIRE(tokens.size() == 0);
    }
    
    SECTION("Only whitespace") {
        auto tokens = split_line("     \t    ");
        REQUIRE(tokens.size() == 0);
    }
    
    SECTION("Unicode handling") {
        auto tokens = split_line("echo \"Hello 世界 🌍\"");
        
        REQUIRE(tokens.size() == 2);
        REQUIRE(tokens[1] == "Hello 世界 🌍");
        
        StdoutCapture capture;
        int result = run_builtin(tokens);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }
    
    SECTION("Special characters in different contexts") {
        auto tokens1 = split_line("echo $PATH");
        REQUIRE(tokens1[1] == "$PATH");
        
        auto tokens2 = split_line("echo \"$PATH\"");
        REQUIRE(tokens2[1] == "$PATH");
        
        auto tokens3 = split_line("echo '$PATH'");
        REQUIRE(tokens3[1] == "$PATH");
    }
    
    SECTION("Multiple consecutive pipes") {
        auto tokens = split_line("echo hello | | | grep h");
        auto pipeline = parse_pipeline_args(tokens);
        
        // Empty pipeline segments should be skipped
        REQUIRE(pipeline.size() >= 2);
    }
    
    SECTION("Mixed operations") {
        // Parse: echo "test data" > file.txt | grep test
        // This is an invalid command but should parse without crashing
        auto tokens = split_line("echo \"test data\" > file.txt | grep test");
        
        // Should parse successfully even if execution would be invalid
        REQUIRE(tokens.size() > 0);
    }
}

TEST_CASE("Integration - Error handling scenarios", "[integration]") {
    SECTION("Type nonexistent command") {
        auto tokens = split_line("type nonexistent_xyz_command");
        
        int result = run_builtin(tokens);
        REQUIRE(result == EXIT_FAILURE_CODE);
    }
    
    SECTION("Cd to nonexistent directory") {
        auto tokens = split_line("cd /this/path/does/not/exist/anywhere");
        
        int result = run_builtin(tokens);
        REQUIRE(result == EXIT_FAILURE_CODE);
    }
    
    SECTION("Empty builtin invocation") {
        std::vector<std::string> empty_args;
        
        int result = run_builtin(empty_args);
        REQUIRE(result == -1); // Not a builtin
    }
    
    SECTION("get_path for nonexistent") {
        std::string path = get_path("command_that_definitely_does_not_exist_123456789");
        REQUIRE(path.empty());
    }
}

TEST_CASE("Integration - Consistency checks", "[integration]") {
    SECTION("Parser preserves intent") {
        // Test that parsing and reparsing gives same result
        auto tokens1 = split_line("echo hello world");
        std::string reconstructed = tokens1[0];
        for (size_t i = 1; i < tokens1.size(); i++) {
            reconstructed += " " + tokens1[i];
        }
        
        auto tokens2 = split_line(reconstructed);
        REQUIRE(tokens1 == tokens2);
    }
    
    SECTION("Builtin check consistency") {
        std::vector<std::string> builtins = {"echo", "exit", "type", "pwd", "cd", "history"};
        
        for (const auto& cmd : builtins) {
            REQUIRE(is_builtin(cmd));
            
            std::vector<std::string> args = {cmd};
            int result = run_builtin(args);
            // Should not return -1 (not a builtin)
            REQUIRE(result != -1);
        }
    }
    
    SECTION("PATH lookup is stable") {
        std::string path1 = get_path("ls");
        std::string path2 = get_path("ls");
        
        REQUIRE(path1 == path2);
    }
}
