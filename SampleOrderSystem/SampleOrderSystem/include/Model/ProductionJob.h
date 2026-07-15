#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include <nlohmann/json.hpp>

// startedAt only has a value while this job is the queue's front (currently
// producing) item -- waiting jobs carry nullopt until they're promoted.
// See design/phase3-design.md SS2 and design/phase4-design.md SS2.
struct ProductionJob {
    int orderId = 0;
    int sampleId = 0;
    int shortfall = 0;
    int actualProductionQty = 0;
    double totalProductionTimeMin = 0.0;
    std::optional<std::chrono::system_clock::time_point> startedAt;
};

inline void to_json(nlohmann::json& j, const ProductionJob& job) {
    j = nlohmann::json{
        {"orderId", job.orderId},
        {"sampleId", job.sampleId},
        {"shortfall", job.shortfall},
        {"actualProductionQty", job.actualProductionQty},
        {"totalProductionTimeMin", job.totalProductionTimeMin},
    };
    if (job.startedAt) {
        const auto epochSeconds =
            std::chrono::duration_cast<std::chrono::seconds>(job.startedAt->time_since_epoch()).count();
        j["startedAtEpochSeconds"] = epochSeconds;
    } else {
        j["startedAtEpochSeconds"] = nullptr;
    }
}

inline void from_json(const nlohmann::json& j, ProductionJob& job) {
    j.at("orderId").get_to(job.orderId);
    j.at("sampleId").get_to(job.sampleId);
    j.at("shortfall").get_to(job.shortfall);
    j.at("actualProductionQty").get_to(job.actualProductionQty);
    j.at("totalProductionTimeMin").get_to(job.totalProductionTimeMin);

    if (j.at("startedAtEpochSeconds").is_null()) {
        job.startedAt = std::nullopt;
    } else {
        const int64_t epochSeconds = j.at("startedAtEpochSeconds").get<int64_t>();
        job.startedAt = std::chrono::system_clock::time_point(std::chrono::seconds(epochSeconds));
    }
}
