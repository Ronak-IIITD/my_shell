#include "catch_amalgamated.hpp"
#include "redirection.h"
#include "utils.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace shell;

TEST_CASE("Redirection - parse_redirection", "[redirection]") {
    SECTION("No redirection") {
        std::vector<std::string> args = {"echo", "hello", "world"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.target_fd == -1);
        REQUIRE(info.filename == "");
        REQUIRE_FALSE(info.append);
        REQUIRE(args.size() == 3); // Args unchanged
    }

    SECTION("Simple stdout redirection") {
        std::vector<std::string> args = {"echo", "hello", ">", "output.txt"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.target_fd == 1);
        REQUIRE(info.filename == "output.txt");
        REQUIRE_FALSE(info.append);
        REQUIRE(args.size() == 2); // > and filename removed
        REQUIRE(args[0] == "echo");
        REQUIRE(args[1] == "hello");
    }

    SECTION("Stdout append redirection") {
        std::vector<std::string> args = {"echo", "hello", ">>", "output.txt"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.target_fd == 1);
        REQUIRE(info.filename == "output.txt");
        REQUIRE(info.append);
        REQUIRE(args.size() == 2);
    }

    SECTION("Explicit stdout redirection (1>)") {
        std::vector<std::string> args = {"echo", "hello", "1>", "output.txt"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.target_fd == 1);
        REQUIRE(info.filename == "output.txt");
        REQUIRE_FALSE(info.append);
    }

    SECTION("Stderr redirection (2>)") {
        std::vector<std::string> args = {"command", "2>", "error.txt"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.target_fd == 2);
        REQUIRE(info.filename == "error.txt");
        REQUIRE_FALSE(info.append);
    }

    SECTION("Stderr append (2>>)") {
        std::vector<std::string> args = {"command", "2>>", "error.txt"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.target_fd == 2);
        REQUIRE(info.filename == "error.txt");
        REQUIRE(info.append);
    }

    SECTION("Redirection at beginning") {
        std::vector<std::string> args = {">", "output.txt", "echo", "hello"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.target_fd == 1);
        REQUIRE(info.filename == "output.txt");
        REQUIRE(args.size() == 2);
        REQUIRE(args[0] == "echo");
    }

    SECTION("Redirection in middle") {
        std::vector<std::string> args = {"echo", ">", "output.txt", "hello"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.target_fd == 1);
        REQUIRE(info.filename == "output.txt");
    }

    SECTION("Missing filename") {
        std::vector<std::string> args = {"echo", "hello", ">"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.target_fd == -1); // Invalid
    }

    SECTION("Multiple redirections - first wins") {
        std::vector<std::string> args = {"echo", ">", "file1.txt", ">>", "file2.txt"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.filename == "file1.txt");
        REQUIRE_FALSE(info.append);
    }

    SECTION("Filename with path") {
        std::vector<std::string> args = {"echo", ">", "/tmp/test/output.txt"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.filename == "/tmp/test/output.txt");
    }

    SECTION("Filename with spaces (already quoted)") {
        std::vector<std::string> args = {"echo", ">", "my file.txt"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.filename == "my file.txt");
    }
}

TEST_CASE("Redirection - apply and restore", "[redirection]") {
    fs::path temp_dir = fs::temp_directory_path();
    
    SECTION("Apply stdout redirection") {
        fs::path temp_file = temp_dir / "test_redirect.txt";
        
        RedirectionInfo info;
        info.target_fd = 1;
        info.filename = temp_file.string();
        info.append = false;
        
        int saved_fd = apply_redirection(info);
        REQUIRE(saved_fd != -1);
        
        // Write to stdout (which is now redirected)
        std::cout << "test output" << std::flush;
        
        // Restore
        restore_redirection(saved_fd, 1);
        
        // Check file contents
        std::ifstream file(temp_file);
        std::string content;
        std::getline(file, content);
        REQUIRE(content == "test output");
        
        fs::remove(temp_file);
    }

    SECTION("Append mode") {
        fs::path temp_file = temp_dir / "test_append.txt";
        
        // Write initial content
        {
            std::ofstream file(temp_file);
            file << "line1" << std::endl;
        }
        
        RedirectionInfo info;
        info.target_fd = 1;
        info.filename = temp_file.string();
        info.append = true;
        
        int saved_fd = apply_redirection(info);
        std::cout << "line2" << std::flush;
        restore_redirection(saved_fd, 1);
        
        // Check both lines exist
        std::ifstream file(temp_file);
        std::string line1, line2;
        std::getline(file, line1);
        std::getline(file, line2);
        REQUIRE(line1 == "line1");
        REQUIRE(line2 == "line2");
        
        fs::remove(temp_file);
    }

    SECTION("Truncate mode overwrites") {
        fs::path temp_file = temp_dir / "test_truncate.txt";
        
        // Write initial content
        {
            std::ofstream file(temp_file);
            file << "old content" << std::endl;
        }
        
        RedirectionInfo info;
        info.target_fd = 1;
        info.filename = temp_file.string();
        info.append = false;
        
        int saved_fd = apply_redirection(info);
        std::cout << "new" << std::flush;
        restore_redirection(saved_fd, 1);
        
        std::ifstream file(temp_file);
        std::string content;
        std::getline(file, content);
        REQUIRE(content == "new");
        
        fs::remove(temp_file);
    }

    SECTION("Invalid file path") {
        RedirectionInfo info;
        info.target_fd = 1;
        info.filename = "/nonexistent/directory/file.txt";
        info.append = false;
        
        int saved_fd = apply_redirection(info);
        REQUIRE(saved_fd == -1); // Should fail
    }

    SECTION("No redirection") {
        RedirectionInfo info;
        info.target_fd = -1;
        info.filename = "";
        info.append = false;
        
        int saved_fd = apply_redirection(info);
        REQUIRE(saved_fd == -1); // No redirection
    }

    SECTION("Multiple redirections in sequence") {
        fs::path file1 = temp_dir / "test1.txt";
        fs::path file2 = temp_dir / "test2.txt";
        
        RedirectionInfo info1;
        info1.target_fd = 1;
        info1.filename = file1.string();
        info1.append = false;
        
        int saved1 = apply_redirection(info1);
        std::cout << "output1" << std::flush;
        restore_redirection(saved1, 1);
        
        RedirectionInfo info2;
        info2.target_fd = 1;
        info2.filename = file2.string();
        info2.append = false;
        
        int saved2 = apply_redirection(info2);
        std::cout << "output2" << std::flush;
        restore_redirection(saved2, 1);
        
        // Check both files
        std::ifstream f1(file1);
        std::string content1;
        std::getline(f1, content1);
        REQUIRE(content1 == "output1");
        
        std::ifstream f2(file2);
        std::string content2;
        std::getline(f2, content2);
        REQUIRE(content2 == "output2");
        
        fs::remove(file1);
        fs::remove(file2);
    }

    SECTION("Restore with invalid fd") {
        // Should handle gracefully
        restore_redirection(-1, 1);
        // No assertions, just shouldn't crash
    }
}

TEST_CASE("Redirection - Edge cases", "[redirection]") {
    SECTION("Very long filename") {
        std::string long_name(1000, 'a');
        std::vector<std::string> args = {"echo", ">", long_name};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.filename == long_name);
    }

    SECTION("Filename with special characters") {
        std::vector<std::string> args = {"echo", ">", "file@#$.txt"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.filename == "file@#$.txt");
    }

    SECTION("Empty args vector") {
        std::vector<std::string> args;
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.target_fd == -1);
    }

    SECTION("Only redirection operator") {
        std::vector<std::string> args = {">"};
        RedirectionInfo info = parse_redirection(args);
        
        REQUIRE(info.target_fd == -1);
    }

    SECTION("Redirection to existing file") {
        fs::path temp_file = fs::temp_directory_path() / "existing.txt";
        
        // Create file
        {
            std::ofstream file(temp_file);
            file << "existing";
        }
        
        RedirectionInfo info;
        info.target_fd = 1;
        info.filename = temp_file.string();
        info.append = false;
        
        int saved_fd = apply_redirection(info);
        REQUIRE(saved_fd != -1);
        
        std::cout << "new" << std::flush;
        restore_redirection(saved_fd, 1);
        
        // Old content should be gone
        std::ifstream file(temp_file);
        std::string content;
        std::getline(file, content);
        REQUIRE(content == "new");
        
        fs::remove(temp_file);
    }
}
