#include "testing.h"

#include <sstream>

#include "Console/ConsoleIO.h"

TEST(consoleIO_readLineReturnsInputAndEchoesPrompt) {
    std::istringstream in("hello\n");
    std::ostringstream out;
    ConsoleIO io(in, out);

    const std::string line = io.readLine("Name > ");
    EXPECT_EQ(line, "hello");
    EXPECT_TRUE(out.str().find("Name > ") != std::string::npos);
}

TEST(consoleIO_readLineThrowsEofExceptionAtStreamEnd) {
    std::istringstream in("");
    std::ostringstream out;
    ConsoleIO io(in, out);

    bool threw = false;
    try {
        io.readLine("Name > ");
    } catch (const EofException&) {
        threw = true;
    }
    EXPECT_TRUE(threw);
}

TEST(consoleIO_readIntRepromptsOnInvalidInput) {
    std::istringstream in("abc\n12abc\n42\n");
    std::ostringstream out;
    ConsoleIO io(in, out);

    EXPECT_EQ(io.readInt("Num > "), 42);
}

TEST(consoleIO_readDoubleAcceptsDecimal) {
    std::istringstream in("3.5\n");
    std::ostringstream out;
    ConsoleIO io(in, out);

    EXPECT_EQ(io.readDouble("Num > "), 3.5);
}

TEST(consoleIO_readOptionalIntEmptyLineReturnsNullopt) {
    std::istringstream in("\n");
    std::ostringstream out;
    ConsoleIO io(in, out);

    const auto value = io.readOptionalInt("Num > ");
    EXPECT_TRUE(!value.has_value());
}

TEST(consoleIO_readOptionalIntRepromptsOnInvalidNonEmptyInput) {
    std::istringstream in("abc\n7\n");
    std::ostringstream out;
    ConsoleIO io(in, out);

    const auto value = io.readOptionalInt("Num > ");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 7);
}

TEST(consoleIO_readOptionalDoubleEmptyLineReturnsNullopt) {
    std::istringstream in("\n");
    std::ostringstream out;
    ConsoleIO io(in, out);

    const auto value = io.readOptionalDouble("Num > ");
    EXPECT_TRUE(!value.has_value());
}
