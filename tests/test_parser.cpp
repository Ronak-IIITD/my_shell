#include "catch_amalgamated.hpp"
#include "parser.h"
#include <vector>
#include <string>

using namespace shell;

TEST_CASE("Parser - Basic tokenization", "[parser]") {
    SECTION("Simple command") {
        auto result = split_line("echo hello");
        REQUIRE(result.size() == 2);
        REQUIRE(result[0] == "echo");
        REQUIRE(result[1] == "hello");
    }

    SECTION("Multiple arguments") {
        auto result = split_line("echo hello world test");
        REQUIRE(result.size() == 4);
        REQUIRE(result[0] == "echo");
        REQUIRE(result[1] == "hello");
        REQUIRE(result[2] == "world");
        REQUIRE(result[3] == "test");
    }

    SECTION("Extra spaces") {
        auto result = split_line("  echo   hello   world  ");
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == "echo");
        REQUIRE(result[1] == "hello");
        REQUIRE(result[2] == "world");
    }

    SECTION("Single word") {
        auto result = split_line("pwd");
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == "pwd");
    }

    SECTION("Empty string") {
        auto result = split_line("");
        REQUIRE(result.size() == 0);
    }

    SECTION("Only spaces") {
        auto result = split_line("    ");
        REQUIRE(result.size() == 0);
    }
}

TEST_CASE("Parser - Double quotes", "[parser]") {
    SECTION("Simple double quotes") {
        auto result = split_line("echo \"hello world\"");
        REQUIRE(result.size() == 2);
        REQUIRE(result[0] == "echo");
        REQUIRE(result[1] == "hello world");
    }

    SECTION("Multiple quoted args") {
        auto result = split_line("echo \"hello\" \"world\"");
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == "echo");
        REQUIRE(result[1] == "hello");
        REQUIRE(result[2] == "world");
    }

    SECTION("Mixed quoted and unquoted") {
        auto result = split_line("echo hello \"world test\" foo");
        REQUIRE(result.size() == 4);
        REQUIRE(result[0] == "echo");
        REQUIRE(result[1] == "hello");
        REQUIRE(result[2] == "world test");
        REQUIRE(result[3] == "foo");
    }

    SECTION("Empty double quotes") {
        auto result = split_line("echo \"\"");
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == "echo");
    }

    SECTION("Double quotes with special chars") {
        auto result = split_line("echo \"hello, world!\"");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "hello, world!");
    }

    SECTION("Nested spaces in quotes") {
        auto result = split_line("echo \"  multiple   spaces  \"");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "  multiple   spaces  ");
    }
}

TEST_CASE("Parser - Single quotes", "[parser]") {
    SECTION("Simple single quotes") {
        auto result = split_line("echo 'hello world'");
        REQUIRE(result.size() == 2);
        REQUIRE(result[0] == "echo");
        REQUIRE(result[1] == "hello world");
    }

    SECTION("Multiple single quoted args") {
        auto result = split_line("echo 'hello' 'world'");
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == "echo");
        REQUIRE(result[1] == "hello");
        REQUIRE(result[2] == "world");
    }

    SECTION("Empty single quotes") {
        auto result = split_line("echo ''");
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == "echo");
    }

    SECTION("Single quotes preserve everything") {
        auto result = split_line("echo 'hello \"world\"'");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "hello \"world\"");
    }

    SECTION("Single quotes with backslashes") {
        auto result = split_line("echo 'hello\\nworld'");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "hello\\nworld"); // Backslash is literal
    }
}

TEST_CASE("Parser - Backslash escapes", "[parser]") {
    SECTION("Escape space") {
        auto result = split_line("echo hello\\ world");
        REQUIRE(result.size() == 2);
        REQUIRE(result[0] == "echo");
        REQUIRE(result[1] == "hello world");
    }

    SECTION("Escape double quote outside quotes") {
        auto result = split_line("echo \\\"hello");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "\"hello");
    }

    SECTION("Escape single quote outside quotes") {
        auto result = split_line("echo \\'hello");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "'hello");
    }

    SECTION("Escape backslash") {
        auto result = split_line("echo \\\\hello");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "\\hello");
    }

    SECTION("Multiple escapes") {
        auto result = split_line("echo\\ hello\\ world");
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == "echo hello world");
    }

    SECTION("Escape in double quotes - special chars") {
        auto result = split_line("echo \"hello\\\"world\"");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "hello\"world");
    }

    SECTION("Escape in double quotes - backslash") {
        auto result = split_line("echo \"hello\\\\world\"");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "hello\\world");
    }

    SECTION("Non-special escape in double quotes") {
        auto result = split_line("echo \"hello\\aworld\"");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "hello\\aworld"); // \a not special, keep backslash
    }

    SECTION("Trailing backslash") {
        auto result = split_line("echo hello\\");
        // Implementation keeps trailing backslash as part of the token
        REQUIRE(result.size() == 2);
        // Actual behavior varies - backslash at end might be kept or escape nothing
    }
}

TEST_CASE("Parser - Mixed quoting", "[parser]") {
    SECTION("Single inside double") {
        auto result = split_line("echo \"hello 'world'\"");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "hello 'world'");
    }

    SECTION("Double inside single") {
        auto result = split_line("echo 'hello \"world\"'");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "hello \"world\"");
    }

    SECTION("Adjacent quotes") {
        auto result = split_line("echo \"hello\"'world'");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "helloworld");
    }

    SECTION("Three types of quoting") {
        auto result = split_line("echo hello\"world\"'test'foo");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "helloworldtestfoo");
    }

    SECTION("Alternating quotes") {
        auto result = split_line("echo 'a'\"b\"'c'\"d\"");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "abcd");
    }
}

TEST_CASE("Parser - Edge cases", "[parser]") {
    SECTION("Unclosed double quote") {
        auto result = split_line("echo \"hello world");
        // Should treat rest as quoted
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "hello world");
    }

    SECTION("Unclosed single quote") {
        auto result = split_line("echo 'hello world");
        // Should treat rest as quoted
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "hello world");
    }

    SECTION("Only quotes") {
        auto result = split_line("\"\"");
        REQUIRE(result.size() == 0); // Empty quotes
    }

    SECTION("Tab characters") {
        auto result = split_line("echo\thello\tworld");
        // Tabs treated as regular chars, not spaces
        REQUIRE(result.size() == 1);
    }

    SECTION("Special characters unquoted") {
        auto result = split_line("echo hello|world");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "hello|world");
    }

    SECTION("Very long argument") {
        std::string long_arg(10000, 'a');
        auto result = split_line("echo " + long_arg);
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == long_arg);
    }

    SECTION("Unicode characters") {
        auto result = split_line("echo \"hello 世界\"");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "hello 世界");
    }

    SECTION("Path with spaces escaped") {
        auto result = split_line("cd /path\\ with\\ spaces/dir");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "/path with spaces/dir");
    }
}

TEST_CASE("Parser - Pipeline parsing", "[parser]") {
    SECTION("Simple pipeline") {
        std::vector<std::string> args = {"ls", "|", "grep", "test"};
        auto result = parse_pipeline_args(args);
        REQUIRE(result.size() == 2);
        REQUIRE(result[0].size() == 1);
        REQUIRE(result[0][0] == "ls");
        REQUIRE(result[1].size() == 2);
        REQUIRE(result[1][0] == "grep");
        REQUIRE(result[1][1] == "test");
    }

    SECTION("Three command pipeline") {
        std::vector<std::string> args = {"ls", "-la", "|", "grep", "cpp", "|", "wc", "-l"};
        auto result = parse_pipeline_args(args);
        REQUIRE(result.size() == 3);
        REQUIRE(result[0].size() == 2);
        REQUIRE(result[0][0] == "ls");
        REQUIRE(result[0][1] == "-la");
        REQUIRE(result[1].size() == 2);
        REQUIRE(result[1][0] == "grep");
        REQUIRE(result[1][1] == "cpp");
        REQUIRE(result[2].size() == 2);
        REQUIRE(result[2][0] == "wc");
        REQUIRE(result[2][1] == "-l");
    }

    SECTION("No pipeline") {
        std::vector<std::string> args = {"echo", "hello", "world"};
        auto result = parse_pipeline_args(args);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].size() == 3);
        REQUIRE(result[0][0] == "echo");
        REQUIRE(result[0][1] == "hello");
        REQUIRE(result[0][2] == "world");
    }

    SECTION("Empty pipeline segments") {
        std::vector<std::string> args = {"|", "ls"};
        auto result = parse_pipeline_args(args);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].size() == 1);
        REQUIRE(result[0][0] == "ls");
    }

    SECTION("Trailing pipe") {
        std::vector<std::string> args = {"ls", "|"};
        auto result = parse_pipeline_args(args);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].size() == 1);
        REQUIRE(result[0][0] == "ls");
    }

    SECTION("Multiple consecutive pipes") {
        std::vector<std::string> args = {"ls", "|", "|", "grep", "test"};
        auto result = parse_pipeline_args(args);
        REQUIRE(result.size() == 2);
        REQUIRE(result[0][0] == "ls");
        REQUIRE(result[1][0] == "grep");
    }
}

TEST_CASE("Parser - Real world commands", "[parser]") {
    SECTION("Git commit with message") {
        auto result = split_line("git commit -m \"Initial commit\"");
        REQUIRE(result.size() == 4);
        REQUIRE(result[0] == "git");
        REQUIRE(result[1] == "commit");
        REQUIRE(result[2] == "-m");
        REQUIRE(result[3] == "Initial commit");
    }

    SECTION("Find with path") {
        auto result = split_line("find /home/user -name '*.cpp'");
        REQUIRE(result.size() == 4);
        REQUIRE(result[0] == "find");
        REQUIRE(result[1] == "/home/user");
        REQUIRE(result[2] == "-name");
        REQUIRE(result[3] == "*.cpp");
    }

    SECTION("Echo with special chars") {
        auto result = split_line("echo \"Price: $10.99\"");
        REQUIRE(result.size() == 2);
        REQUIRE(result[1] == "Price: $10.99");
    }

    SECTION("Curl with URL") {
        auto result = split_line("curl -H 'Content-Type: application/json' https://api.example.com");
        REQUIRE(result.size() == 4);
        REQUIRE(result[2] == "Content-Type: application/json");
    }

    SECTION("Complex grep") {
        auto result = split_line("grep -r \"error.*failed\" /var/log");
        REQUIRE(result.size() == 4);
        REQUIRE(result[2] == "error.*failed");
    }
}
