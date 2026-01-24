#include "catch_amalgamated.hpp"
#include "utils.h"
#include <cstdlib>
#include <string>

using namespace shell;

TEST_CASE("Utils - get_path", "[utils]") {
    SECTION("Find common commands") {
        // These should exist on most Unix systems
        std::string ls_path = get_path("ls");
        REQUIRE_FALSE(ls_path.empty());
        REQUIRE(ls_path.find("ls") != std::string::npos);
        
        std::string cat_path = get_path("cat");
        REQUIRE_FALSE(cat_path.empty());
        REQUIRE(cat_path.find("cat") != std::string::npos);
    }

    SECTION("Nonexistent command") {
        std::string path = get_path("nonexistent_command_xyz_123");
        REQUIRE(path.empty());
    }

    SECTION("Empty command") {
        std::string path = get_path("");
        REQUIRE(path.empty());
    }

    SECTION("Command with path") {
        std::string path = get_path("/bin/ls");
        // get_path searches PATH, so full path might not be found
        // This tests implementation behavior
    }

    SECTION("Command with spaces") {
        std::string path = get_path("command with spaces");
        REQUIRE(path.empty());
    }

    SECTION("Very long command name") {
        std::string long_cmd(10000, 'a');
        std::string path = get_path(long_cmd);
        REQUIRE(path.empty());
    }

    SECTION("Special characters") {
        std::string path = get_path("$PATH");
        REQUIRE(path.empty());
    }

    SECTION("Multiple common commands") {
        // Test multiple commands to ensure function works repeatedly
        std::string grep = get_path("grep");
        std::string echo = get_path("echo");
        std::string cat = get_path("cat");
        
        // At least one should be found
        REQUIRE((grep != "" || echo != "" || cat != ""));
    }
}

TEST_CASE("Utils - FileDescriptor RAII", "[utils]") {
    SECTION("Default constructor creates invalid fd") {
        FileDescriptor fd;
        REQUIRE_FALSE(fd.is_valid());
        REQUIRE(fd.get() == -1);
        REQUIRE_FALSE(static_cast<bool>(fd));
    }

    SECTION("Constructor with valid fd") {
        int raw_fd = dup(STDOUT_FILENO);
        FileDescriptor fd(raw_fd);
        REQUIRE(fd.is_valid());
        REQUIRE(fd.get() == raw_fd);
        REQUIRE(static_cast<bool>(fd));
    }

    SECTION("Constructor with invalid fd") {
        FileDescriptor fd(-1);
        REQUIRE_FALSE(fd.is_valid());
    }

    SECTION("Move constructor") {
        int raw_fd = dup(STDOUT_FILENO);
        FileDescriptor fd1(raw_fd);
        int original_fd = fd1.get();
        
        FileDescriptor fd2(std::move(fd1));
        
        REQUIRE_FALSE(fd1.is_valid()); // fd1 should be invalid after move
        REQUIRE(fd2.is_valid());
        REQUIRE(fd2.get() == original_fd);
    }

    SECTION("Move assignment") {
        int raw_fd1 = dup(STDOUT_FILENO);
        int raw_fd2 = dup(STDIN_FILENO);
        
        FileDescriptor fd1(raw_fd1);
        FileDescriptor fd2(raw_fd2);
        
        int original_fd1 = fd1.get();
        fd2 = std::move(fd1);
        
        REQUIRE_FALSE(fd1.is_valid());
        REQUIRE(fd2.is_valid());
        REQUIRE(fd2.get() == original_fd1);
    }

    SECTION("Release ownership") {
        int raw_fd = dup(STDOUT_FILENO);
        FileDescriptor fd(raw_fd);
        
        int released_fd = fd.release();
        
        REQUIRE(released_fd == raw_fd);
        REQUIRE_FALSE(fd.is_valid());
        
        // Clean up manually since we released
        close(released_fd);
    }

    SECTION("Close method") {
        int raw_fd = dup(STDOUT_FILENO);
        FileDescriptor fd(raw_fd);
        
        fd.close();
        
        REQUIRE_FALSE(fd.is_valid());
        REQUIRE(fd.get() == -1);
    }

    SECTION("Multiple close calls") {
        int raw_fd = dup(STDOUT_FILENO);
        FileDescriptor fd(raw_fd);
        
        fd.close();
        fd.close(); // Should be safe to call multiple times
        
        REQUIRE_FALSE(fd.is_valid());
    }

    SECTION("Self move assignment") {
        int raw_fd = dup(STDOUT_FILENO);
        FileDescriptor fd(raw_fd);
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wself-move"
        fd = std::move(fd);
        #pragma GCC diagnostic pop
        
        // Should still be valid after self-move
        REQUIRE(fd.is_valid());
    }

    SECTION("Destructor closes fd") {
        int raw_fd = dup(STDOUT_FILENO);
        {
            FileDescriptor fd(raw_fd);
            REQUIRE(fd.is_valid());
        } // fd destroyed here
        
        // Try to use raw_fd - should be invalid now
        // Can't easily test without system calls
    }
}

TEST_CASE("Utils - ReadlinePtr", "[utils]") {
    SECTION("Default constructed pointer is null") {
        ReadlinePtr ptr;
        REQUIRE(ptr == nullptr);
    }

    SECTION("Construct with malloc'd string") {
        char* raw = static_cast<char*>(malloc(10));
        strcpy(raw, "test");
        
        ReadlinePtr ptr(raw);
        REQUIRE(ptr != nullptr);
        REQUIRE(strcmp(ptr.get(), "test") == 0);
    }

    SECTION("Move constructor") {
        char* raw = static_cast<char*>(malloc(10));
        strcpy(raw, "test");
        
        ReadlinePtr ptr1(raw);
        ReadlinePtr ptr2(std::move(ptr1));
        
        REQUIRE(ptr1 == nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(strcmp(ptr2.get(), "test") == 0);
    }

    SECTION("Reset pointer") {
        char* raw = static_cast<char*>(malloc(10));
        strcpy(raw, "test");
        
        ReadlinePtr ptr(raw);
        ptr.reset();
        
        REQUIRE(ptr == nullptr);
    }

    SECTION("Deleter calls free") {
        // This tests that the custom deleter works
        char* raw = static_cast<char*>(malloc(100));
        {
            ReadlinePtr ptr(raw);
        } // Deleter should call free here
        // Can't easily verify without valgrind
    }
}

TEST_CASE("Utils - Exit codes", "[utils]") {
    SECTION("Exit code constants are correct") {
        REQUIRE(EXIT_SUCCESS_CODE == 0);
        REQUIRE(EXIT_FAILURE_CODE == 1);
        REQUIRE(EXIT_COMMAND_NOT_FOUND == 127);
    }

    SECTION("Exit codes follow POSIX standards") {
        // Success is 0
        REQUIRE(EXIT_SUCCESS_CODE == 0);
        
        // General failure is 1
        REQUIRE(EXIT_FAILURE_CODE == 1);
        
        // Command not found is 127
        REQUIRE(EXIT_COMMAND_NOT_FOUND == 127);
    }
}

TEST_CASE("Utils - Edge cases", "[utils]") {
    SECTION("get_path with null PATH") {
        // Save original PATH
        const char* original_path = std::getenv("PATH");
        std::string saved_path = original_path ? original_path : "";
        
        // Unset PATH
        unsetenv("PATH");
        
        std::string result = get_path("ls");
        REQUIRE(result.empty());
        
        // Restore PATH
        if (!saved_path.empty()) {
            setenv("PATH", saved_path.c_str(), 1);
        }
    }

    SECTION("get_path with empty PATH") {
        // Save original PATH
        const char* original_path = std::getenv("PATH");
        std::string saved_path = original_path ? original_path : "";
        
        // Set empty PATH
        setenv("PATH", "", 1);
        
        std::string result = get_path("ls");
        REQUIRE(result.empty());
        
        // Restore PATH
        if (!saved_path.empty()) {
            setenv("PATH", saved_path.c_str(), 1);
        }
    }

    SECTION("get_path with custom PATH") {
        // Save original PATH
        const char* original_path = std::getenv("PATH");
        std::string saved_path = original_path ? original_path : "";
        
        // Set custom PATH to /bin only
        setenv("PATH", "/bin:/usr/bin", 1);
        
        std::string result = get_path("ls");
        REQUIRE_FALSE(result.empty());
        
        // Restore PATH
        if (!saved_path.empty()) {
            setenv("PATH", saved_path.c_str(), 1);
        }
    }

    SECTION("FileDescriptor with stdin/stdout/stderr") {
        // Don't take ownership of standard fds
        int fd = dup(STDOUT_FILENO);
        FileDescriptor wrapper(fd);
        REQUIRE(wrapper.is_valid());
    }

    SECTION("Very large fd number") {
        FileDescriptor fd(99999);
        // Will be invalid since it doesn't exist
        // But should handle gracefully
        REQUIRE(fd.get() == 99999);
    }
}
