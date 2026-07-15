#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        registry().push_back({name, fn});
    }
};

inline int runAll() {
    int passed = 0;
    int failed = 0;
    for (const auto& test : registry()) {
        try {
            test.fn();
            std::cout << "[PASS] " << test.name << "\n";
            ++passed;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << test.name << ": " << e.what() << "\n";
            ++failed;
        }
    }
    std::cout << passed << "/" << registry().size() << " tests passed\n";
    return failed == 0 ? 0 : 1;
}

} // namespace testing

#define TESTING_CONCAT_INNER(a, b) a##b
#define TESTING_CONCAT(a, b) TESTING_CONCAT_INNER(a, b)

#define TEST(name)                                                          \
    void name();                                                            \
    static testing::Registrar TESTING_CONCAT(registrar_, name)(#name, name); \
    void name()

#define EXPECT_TRUE(cond)                                                   \
    if (!(cond)) {                                                          \
        throw std::runtime_error("EXPECT_TRUE failed: " #cond);             \
    }

#define EXPECT_EQ(a, b)                                                     \
    if (!((a) == (b))) {                                                    \
        throw std::runtime_error("EXPECT_EQ failed: " #a " == " #b);        \
    }
