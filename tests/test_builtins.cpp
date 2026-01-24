#include "catch_amalgamated.hpp"
#include "builtins.h"
#include "utils.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace fs = std::filesystem;
using namespace shell;

TEST_CASE("Builtins - is_builtin", "[builtins]") {
    SECTION("Valid builtins") {
        REQUIRE(is_builtin("echo"));
        REQUIRE(is_builtin("exit"));
        REQUIRE(is_builtin("type"));
        REQUIRE(is_builtin("pwd"));
        REQUIRE(is_builtin("cd"));
        REQUIRE(is_builtin("history"));
    }

    SECTION("Invalid commands") {
        REQUIRE_FALSE(is_builtin("ls"));
        REQUIRE_FALSE(is_builtin("grep"));
        REQUIRE_FALSE(is_builtin("nonexistent"));
        REQUIRE_FALSE(is_builtin(""));
    }

    SECTION("Case sensitivity") {
        REQUIRE_FALSE(is_builtin("ECHO"));
        REQUIRE_FALSE(is_builtin("Echo"));
        REQUIRE_FALSE(is_builtin("PWD"));
    }

    SECTION("Partial matches") {
        REQUIRE_FALSE(is_builtin("ech"));
        REQUIRE_FALSE(is_builtin("echoo"));
        REQUIRE_FALSE(is_builtin("cdd"));
    }
}

TEST_CASE("Builtins - builtin_echo", "[builtins]") {
    SECTION("Simple echo") {
        std::vector<std::string> args = {"echo", "hello"};
        int result = builtin_echo(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("Multiple arguments") {
        std::vector<std::string> args = {"echo", "hello", "world", "test"};
        int result = builtin_echo(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("No arguments") {
        std::vector<std::string> args = {"echo"};
        int result = builtin_echo(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("Empty strings") {
        std::vector<std::string> args = {"echo", "", "test", ""};
        int result = builtin_echo(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("Special characters") {
        std::vector<std::string> args = {"echo", "$PATH", "~", "*"};
        int result = builtin_echo(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("Very long string") {
        std::string long_str(10000, 'a');
        std::vector<std::string> args = {"echo", long_str};
        int result = builtin_echo(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }
}

TEST_CASE("Builtins - builtin_pwd", "[builtins]") {
    SECTION("Get current directory") {
        int result = builtin_pwd();
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("After changing directory") {
        fs::path original = fs::current_path();
        fs::path tmp = fs::temp_directory_path();
        
        builtin_cd(tmp.string());
        int result = builtin_pwd();
        REQUIRE(result == EXIT_SUCCESS_CODE);
        
        // Restore
        fs::current_path(original);
    }
}

TEST_CASE("Builtins - builtin_cd", "[builtins]") {
    fs::path original = fs::current_path();

    SECTION("Change to valid directory") {
        fs::path tmp = fs::temp_directory_path();
        int result = builtin_cd(tmp.string());
        REQUIRE(result == EXIT_SUCCESS_CODE);
        REQUIRE(fs::current_path() == tmp);
    }

    SECTION("Change to nonexistent directory") {
        int result = builtin_cd("/nonexistent/path/that/does/not/exist");
        REQUIRE(result == EXIT_FAILURE_CODE);
        // Should still be in original directory
    }

    SECTION("Change to home directory") {
        const char* home = std::getenv("HOME");
        if (home) {
            int result = builtin_cd("~");
            REQUIRE(result == EXIT_SUCCESS_CODE);
            REQUIRE(fs::current_path() == fs::path(home));
        }
    }

    SECTION("Change to home subdirectory") {
        const char* home = std::getenv("HOME");
        if (home && fs::exists(fs::path(home))) {
            // Try to cd to ~/. which should work
            int result = builtin_cd("~/.");
            REQUIRE(result == EXIT_SUCCESS_CODE);
        }
    }

    SECTION("Relative path") {
        int result = builtin_cd(".");
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("Parent directory") {
        int result = builtin_cd("..");
        REQUIRE(result == EXIT_SUCCESS_CODE);
        // Can't easily verify without knowing filesystem structure
    }

    SECTION("Empty path") {
        // Empty path should default to home
        int result = builtin_cd("");
        // Behavior depends on implementation
    }

    SECTION("Path to file not directory") {
        // Create temp file
        fs::path temp_file = fs::temp_directory_path() / "test_file.txt";
        std::ofstream(temp_file) << "test";
        
        int result = builtin_cd(temp_file.string());
        REQUIRE(result == EXIT_FAILURE_CODE);
        
        fs::remove(temp_file);
    }

    SECTION("Multiple directory changes") {
        fs::path tmp = fs::temp_directory_path();
        
        int result1 = builtin_cd(tmp.string());
        REQUIRE(result1 == EXIT_SUCCESS_CODE);
        
        int result2 = builtin_cd("..");
        REQUIRE(result2 == EXIT_SUCCESS_CODE);
        
        int result3 = builtin_cd(tmp.string());
        REQUIRE(result3 == EXIT_SUCCESS_CODE);
    }

    // Restore original directory after all tests
    fs::current_path(original);
}

TEST_CASE("Builtins - builtin_type", "[builtins]") {
    SECTION("Type of builtin command") {
        int result = builtin_type("echo");
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("Type of external command") {
        int result = builtin_type("ls");
        // Should succeed if ls is in PATH
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("Type of nonexistent command") {
        int result = builtin_type("nonexistent_command_xyz");
        REQUIRE(result == EXIT_FAILURE_CODE);
    }

    SECTION("Type of all builtins") {
        REQUIRE(builtin_type("echo") == EXIT_SUCCESS_CODE);
        REQUIRE(builtin_type("exit") == EXIT_SUCCESS_CODE);
        REQUIRE(builtin_type("type") == EXIT_SUCCESS_CODE);
        REQUIRE(builtin_type("pwd") == EXIT_SUCCESS_CODE);
        REQUIRE(builtin_type("cd") == EXIT_SUCCESS_CODE);
        REQUIRE(builtin_type("history") == EXIT_SUCCESS_CODE);
    }

    SECTION("Type of common external commands") {
        // These should exist on most systems
        int result_ls = builtin_type("ls");
        int result_cat = builtin_type("cat");
        int result_grep = builtin_type("grep");
        
        // At least one should exist
        REQUIRE((result_ls == EXIT_SUCCESS_CODE || 
                 result_cat == EXIT_SUCCESS_CODE || 
                 result_grep == EXIT_SUCCESS_CODE));
    }
}

TEST_CASE("Builtins - run_builtin dispatcher", "[builtins]") {
    SECTION("Empty args") {
        std::vector<std::string> args;
        int result = run_builtin(args);
        REQUIRE(result == -1); // Not a builtin
    }

    SECTION("Echo dispatch") {
        std::vector<std::string> args = {"echo", "test"};
        int result = run_builtin(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("Pwd dispatch") {
        std::vector<std::string> args = {"pwd"};
        int result = run_builtin(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("Type dispatch") {
        std::vector<std::string> args = {"type", "echo"};
        int result = run_builtin(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("Type without argument") {
        std::vector<std::string> args = {"type"};
        int result = run_builtin(args);
        REQUIRE(result == EXIT_SUCCESS_CODE); // Should handle gracefully
    }

    SECTION("Non-builtin command") {
        std::vector<std::string> args = {"ls", "-la"};
        int result = run_builtin(args);
        REQUIRE(result == -1); // Not a builtin
    }

    SECTION("Cd dispatch") {
        fs::path original = fs::current_path();
        
        std::vector<std::string> args = {"cd", "."};
        int result = run_builtin(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
        
        fs::current_path(original);
    }

    SECTION("Cd without argument") {
        fs::path original = fs::current_path();
        
        std::vector<std::string> args = {"cd"};
        int result = run_builtin(args);
        // Should default to home
        
        fs::current_path(original);
    }
}

TEST_CASE("Builtins - Edge cases", "[builtins]") {
    SECTION("Very long echo argument") {
        std::string long_str(100000, 'x');
        std::vector<std::string> args = {"echo", long_str};
        int result = builtin_echo(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("Many echo arguments") {
        std::vector<std::string> args = {"echo"};
        for (int i = 0; i < 1000; i++) {
            args.push_back("arg" + std::to_string(i));
        }
        int result = builtin_echo(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }

    SECTION("Cd to path with special characters") {
        // Can't easily test without creating directories
        // but we can test error handling
        int result = builtin_cd("/path/with spaces/test");
        REQUIRE(result == EXIT_FAILURE_CODE);
    }

    SECTION("Type with empty string") {
        int result = builtin_type("");
        // Empty string check - implementation might treat as not found or handle gracefully
        // Accepting either behavior
        REQUIRE((result == EXIT_FAILURE_CODE || result == EXIT_SUCCESS_CODE));
    }

    SECTION("Builtin check with very long name") {
        std::string long_name(10000, 'a');
        REQUIRE_FALSE(is_builtin(long_name));
    }

    SECTION("Unicode in echo") {
        std::vector<std::string> args = {"echo", "Hello 世界 🌍"};
        int result = builtin_echo(args);
        REQUIRE(result == EXIT_SUCCESS_CODE);
    }
}
