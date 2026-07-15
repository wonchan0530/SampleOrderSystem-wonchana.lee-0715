#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

// Generic JSON-array-backed file store. Knows nothing about the domain type T
// beyond its to_json/from_json (ADL). Failures are absorbed here (never
// propagated as exceptions) so callers always get a usable, if possibly
// empty, list back.
template <typename T>
class JsonFileStore {
public:
    explicit JsonFileStore(std::filesystem::path filePath) : filePath_(std::move(filePath)) {}

    std::vector<T> load() const {
        std::ifstream in(filePath_);
        if (!in.is_open()) {
            return {};
        }

        std::stringstream buffer;
        buffer << in.rdbuf();
        const std::string content = buffer.str();
        if (content.empty()) {
            return {};
        }

        try {
            const nlohmann::json parsed = nlohmann::json::parse(content);
            return parsed.get<std::vector<T>>();
        } catch (const nlohmann::json::exception& e) {
            std::cerr << "Warning: failed to parse " << filePath_.string() << ": " << e.what() << "\n";
            return {};
        }
    }

    void save(const std::vector<T>& items) const {
        const std::filesystem::path parent = filePath_.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent)) {
            std::filesystem::create_directories(parent);
        }

        const nlohmann::json arr = items;
        std::ofstream out(filePath_, std::ios::trunc);
        out << arr.dump(2);
    }

private:
    std::filesystem::path filePath_;
};
