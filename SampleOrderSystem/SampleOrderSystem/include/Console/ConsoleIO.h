#pragma once

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "Repository/RepositoryResult.h"

// Thrown by readLine (and anything built on it) when the input stream hits
// EOF. Every menu's run() loop catches this exactly once at its own top
// level, so no individual read call site needs to check stream state.
class EofException : public std::runtime_error {
public:
    EofException() : std::runtime_error("input stream reached EOF") {}
};

// Streams are injected (default std::cin/std::cout) so console flows can be
// driven end-to-end in tests via istringstream/ostringstream.
class ConsoleIO {
public:
    explicit ConsoleIO(std::istream& in = std::cin, std::ostream& out = std::cout);

    void println(const std::string& text = "");

    std::string readLine(const std::string& prompt);
    std::optional<std::string> readOptionalLine(const std::string& prompt);

    int readInt(const std::string& prompt);
    double readDouble(const std::string& prompt);
    std::optional<int> readOptionalInt(const std::string& prompt);
    std::optional<double> readOptionalDouble(const std::string& prompt);

    void printResult(const RepositoryResult& result);

private:
    std::istream& in_;
    std::ostream& out_;
};
