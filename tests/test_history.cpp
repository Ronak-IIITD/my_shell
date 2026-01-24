#include "catch_amalgamated.hpp"
#include "history.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace fs = std::filesystem;
using namespace shell;

TEST_CASE("History - Initialization", "[history]") {
    SECTION("Init clears history") {
        add_to_history("test command");
        init_history();
        
        const auto& hist = get_command_history();
        REQUIRE(hist.size() == 0);
        REQUIRE(get_history_file_index() == 0);
    }
}

TEST_CASE("History - Adding commands", "[history]") {
    init_history();
    
    SECTION("Add single command") {
        add_to_history("echo hello");
        
        const auto& hist = get_command_history();
        REQUIRE(hist.size() == 1);
        REQUIRE(hist[0] == "echo hello");
    }

    SECTION("Add multiple commands") {
        add_to_history("pwd");
        add_to_history("ls -la");
        add_to_history("cd /tmp");
        
        const auto& hist = get_command_history();
        REQUIRE(hist.size() == 3);
        REQUIRE(hist[0] == "pwd");
        REQUIRE(hist[1] == "ls -la");
        REQUIRE(hist[2] == "cd /tmp");
    }

    SECTION("Add empty command") {
        size_t before = get_command_history().size();
        add_to_history("");
        size_t after = get_command_history().size();
        
        // Empty commands should not be added
        REQUIRE(before == after);
    }

    SECTION("Add command with spaces") {
        add_to_history("  echo test  ");
        
        const auto& hist = get_command_history();
        REQUIRE(hist.size() == 1);
        REQUIRE(hist[0] == "  echo test  ");
    }

    SECTION("Add many commands") {
        for (int i = 0; i < 1000; i++) {
            add_to_history("command" + std::to_string(i));
        }
        
        const auto& hist = get_command_history();
        REQUIRE(hist.size() == 1000);
        REQUIRE(hist[0] == "command0");
        REQUIRE(hist[999] == "command999");
    }

    SECTION("Add duplicate commands") {
        add_to_history("echo hello");
        add_to_history("echo hello");
        add_to_history("echo hello");
        
        const auto& hist = get_command_history();
        // Should allow duplicates
        REQUIRE(hist.size() == 3);
    }
}

TEST_CASE("History - File operations", "[history]") {
    init_history();
    fs::path temp_dir = fs::temp_directory_path();
    fs::path hist_file = temp_dir / "test_history.txt";
    
    // Save original HISTFILE
    const char* original_histfile = std::getenv("HISTFILE");
    std::string saved_histfile = original_histfile ? original_histfile : "";
    
    // Set test HISTFILE
    setenv("HISTFILE", hist_file.string().c_str(), 1);
    
    SECTION("Load empty history file") {
        // Create empty file
        std::ofstream(hist_file);
        
        load_history_from_file();
        
        const auto& hist = get_command_history();
        REQUIRE(hist.size() == 0);
    }
    
    SECTION("Load history from file") {
        // Create history file
        {
            std::ofstream file(hist_file);
            file << "command1" << std::endl;
            file << "command2" << std::endl;
            file << "command3" << std::endl;
        }
        
        load_history_from_file();
        
        const auto& hist = get_command_history();
        REQUIRE(hist.size() == 3);
        REQUIRE(hist[0] == "command1");
        REQUIRE(hist[1] == "command2");
        REQUIRE(hist[2] == "command3");
        REQUIRE(get_history_file_index() == 3); // All from file
    }
    
    SECTION("Save history to file") {
        add_to_history("new1");
        add_to_history("new2");
        
        save_history_to_file();
        
        // Read file
        std::ifstream file(hist_file);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        
        REQUIRE(lines.size() == 2);
        REQUIRE(lines[0] == "new1");
        REQUIRE(lines[1] == "new2");
    }
    
    SECTION("Save only new commands") {
        // Load existing
        {
            std::ofstream file(hist_file);
            file << "old1" << std::endl;
            file << "old2" << std::endl;
        }
        
        load_history_from_file();
        add_to_history("new1");
        add_to_history("new2");
        
        save_history_to_file();
        
        // Read file
        std::ifstream file(hist_file);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        
        // Should have old + new
        REQUIRE(lines.size() == 4);
        REQUIRE(lines[0] == "old1");
        REQUIRE(lines[1] == "old2");
        REQUIRE(lines[2] == "new1");
        REQUIRE(lines[3] == "new2");
    }
    
    SECTION("HISTFILE not set") {
        unsetenv("HISTFILE");
        
        add_to_history("test");
        save_history_to_file(); // Should not crash
        
        // No file should be created
    }
    
    SECTION("Load from nonexistent file") {
        fs::remove(hist_file);
        
        load_history_from_file(); // Should not crash
        
        const auto& hist = get_command_history();
        REQUIRE(hist.size() == 0);
    }
    
    // Cleanup
    fs::remove(hist_file);
    
    // Restore HISTFILE
    if (!saved_histfile.empty()) {
        setenv("HISTFILE", saved_histfile.c_str(), 1);
    } else {
        unsetenv("HISTFILE");
    }
}

TEST_CASE("History - Index management", "[history]") {
    init_history();
    
    SECTION("Initial index is 0") {
        REQUIRE(get_history_file_index() == 0);
    }
    
    SECTION("Set history index") {
        set_history_file_index(10);
        REQUIRE(get_history_file_index() == 10);
    }
    
    SECTION("Index after loading") {
        fs::path temp_file = fs::temp_directory_path() / "test_idx.txt";
        
        {
            std::ofstream file(temp_file);
            file << "cmd1" << std::endl;
            file << "cmd2" << std::endl;
        }
        
        const char* original = std::getenv("HISTFILE");
        setenv("HISTFILE", temp_file.string().c_str(), 1);
        
        load_history_from_file();
        
        REQUIRE(get_history_file_index() == 2);
        
        if (original) {
            setenv("HISTFILE", original, 1);
        } else {
            unsetenv("HISTFILE");
        }
        
        fs::remove(temp_file);
    }
}

TEST_CASE("History - Edge cases", "[history]") {
    init_history();
    
    SECTION("Very long command") {
        std::string long_cmd(100000, 'a');
        add_to_history(long_cmd);
        
        const auto& hist = get_command_history();
        REQUIRE(hist.size() == 1);
        REQUIRE(hist[0] == long_cmd);
    }
    
    SECTION("Commands with newlines") {
        add_to_history("line1\nline2");
        
        const auto& hist = get_command_history();
        REQUIRE(hist.size() == 1);
        REQUIRE(hist[0] == "line1\nline2");
    }
    
    SECTION("Unicode commands") {
        add_to_history("echo 世界");
        
        const auto& hist = get_command_history();
        REQUIRE(hist.size() == 1);
        REQUIRE(hist[0] == "echo 世界");
    }
    
    SECTION("Special characters") {
        add_to_history("echo $PATH | grep /bin");
        
        const auto& hist = get_command_history();
        REQUIRE(hist[0] == "echo $PATH | grep /bin");
    }
    
    SECTION("File with empty lines") {
        fs::path temp_file = fs::temp_directory_path() / "test_empty.txt";
        
        {
            std::ofstream file(temp_file);
            file << "cmd1" << std::endl;
            file << "" << std::endl;
            file << "cmd2" << std::endl;
            file << "" << std::endl;
        }
        
        const char* original = std::getenv("HISTFILE");
        setenv("HISTFILE", temp_file.string().c_str(), 1);
        
        load_history_from_file();
        
        const auto& hist = get_command_history();
        // Empty lines should be skipped
        REQUIRE(hist.size() == 2);
        REQUIRE(hist[0] == "cmd1");
        REQUIRE(hist[1] == "cmd2");
        
        if (original) {
            setenv("HISTFILE", original, 1);
        } else {
            unsetenv("HISTFILE");
        }
        
        fs::remove(temp_file);
    }
}

TEST_CASE("History - Get command history const correctness", "[history]") {
    init_history();
    add_to_history("test");
    
    SECTION("Returns const reference") {
        const auto& hist = get_command_history();
        REQUIRE(hist.size() == 1);
        
        // This should compile (const access)
        std::string cmd = hist[0];
        REQUIRE(cmd == "test");
        
        // Uncommenting below should cause compile error (can't modify const ref):
        // hist.push_back("should not compile");
    }
}
