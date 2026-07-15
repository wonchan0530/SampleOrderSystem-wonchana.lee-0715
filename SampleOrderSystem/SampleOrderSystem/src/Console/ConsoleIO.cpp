#include "Console/ConsoleIO.h"

namespace {

template <typename T, typename ParseFn>
std::optional<T> parseFull(const std::string& line, ParseFn parseFn) {
    if (line.empty()) {
        return std::nullopt;
    }
    try {
        size_t pos = 0;
        T value = parseFn(line, &pos);
        if (pos != line.size()) {
            return std::nullopt;
        }
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<int> parseInt(const std::string& line) {
    return parseFull<int>(line, [](const std::string& s, size_t* pos) { return std::stoi(s, pos); });
}

std::optional<double> parseDouble(const std::string& line) {
    return parseFull<double>(line, [](const std::string& s, size_t* pos) { return std::stod(s, pos); });
}

}  // namespace

ConsoleIO::ConsoleIO(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

void ConsoleIO::println(const std::string& text) {
    out_ << text << "\n";
}

std::string ConsoleIO::readLine(const std::string& prompt) {
    out_ << prompt;
    std::string line;
    if (!std::getline(in_, line)) {
        throw EofException();
    }
    return line;
}

std::optional<std::string> ConsoleIO::readOptionalLine(const std::string& prompt) {
    const std::string line = readLine(prompt);
    if (line.empty()) {
        return std::nullopt;
    }
    return line;
}

int ConsoleIO::readInt(const std::string& prompt) {
    while (true) {
        const std::string line = readLine(prompt);
        if (const auto value = parseInt(line)) {
            return *value;
        }
        out_ << "숫자를 정확히 입력해주세요.\n";
    }
}

double ConsoleIO::readDouble(const std::string& prompt) {
    while (true) {
        const std::string line = readLine(prompt);
        if (const auto value = parseDouble(line)) {
            return *value;
        }
        out_ << "숫자를 정확히 입력해주세요.\n";
    }
}

std::optional<int> ConsoleIO::readOptionalInt(const std::string& prompt) {
    while (true) {
        const std::string line = readLine(prompt);
        if (line.empty()) {
            return std::nullopt;
        }
        if (const auto value = parseInt(line)) {
            return value;
        }
        out_ << "숫자를 정확히 입력해주세요. (건너뛰려면 빈 값 입력)\n";
    }
}

std::optional<double> ConsoleIO::readOptionalDouble(const std::string& prompt) {
    while (true) {
        const std::string line = readLine(prompt);
        if (line.empty()) {
            return std::nullopt;
        }
        if (const auto value = parseDouble(line)) {
            return value;
        }
        out_ << "숫자를 정확히 입력해주세요. (건너뛰려면 빈 값 입력)\n";
    }
}

void ConsoleIO::printResult(const RepositoryResult& result) {
    out_ << result.message << "\n";
}
