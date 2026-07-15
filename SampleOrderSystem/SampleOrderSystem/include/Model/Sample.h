#pragma once

#include <string>

#include <nlohmann/json.hpp>

struct Sample {
    int id = 0;
    std::string name;
    double avgProductionTimeMin = 0.0;
    double yieldRate = 0.0;
    int stock = 0;
};

inline void to_json(nlohmann::json& j, const Sample& s) {
    j = nlohmann::json{
        {"id", s.id},
        {"name", s.name},
        {"avgProductionTimeMin", s.avgProductionTimeMin},
        {"yieldRate", s.yieldRate},
        {"stock", s.stock},
    };
}

inline void from_json(const nlohmann::json& j, Sample& s) {
    j.at("id").get_to(s.id);
    j.at("name").get_to(s.name);
    j.at("avgProductionTimeMin").get_to(s.avgProductionTimeMin);
    j.at("yieldRate").get_to(s.yieldRate);
    j.at("stock").get_to(s.stock);
}
