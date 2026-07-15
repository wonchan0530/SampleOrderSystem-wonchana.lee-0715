#include "Repository/SampleRepository.h"

#include <algorithm>
#include <cctype>

#include "Repository/NextIdCalculator.h"

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

}  // namespace

SampleRepository::SampleRepository(std::filesystem::path dataFile) : store_(std::move(dataFile)) {
    cache_ = store_.load();
    recalcNextId();
}

void SampleRepository::recalcNextId() {
    nextId_ = computeNextId(cache_, [](const Sample& s) { return s.id; });
}

void SampleRepository::persist() const {
    store_.save(cache_);
}

std::vector<Sample> SampleRepository::findAll() const {
    return cache_;
}

std::optional<Sample> SampleRepository::findById(int id) const {
    auto it = std::find_if(cache_.begin(), cache_.end(), [id](const Sample& s) { return s.id == id; });
    if (it == cache_.end()) {
        return std::nullopt;
    }
    return *it;
}

std::vector<Sample> SampleRepository::search(const std::string& keyword) const {
    std::vector<Sample> result;
    const std::string lowerKeyword = toLower(keyword);
    for (const auto& s : cache_) {
        const bool idMatch = std::to_string(s.id) == keyword;
        const bool nameMatch = toLower(s.name).find(lowerKeyword) != std::string::npos;
        if (idMatch || nameMatch) {
            result.push_back(s);
        }
    }
    return result;
}

RepositoryResult SampleRepository::validateFields(double avgProductionTimeMin, double yieldRate) {
    if (avgProductionTimeMin <= 0.0) {
        return {false, "평균 생산시간은 0보다 커야 합니다."};
    }
    if (yieldRate <= 0.0 || yieldRate > 1.0) {
        return {false, "수율은 0 초과 1 이하이어야 합니다."};
    }
    return {true, ""};
}

RepositoryResult SampleRepository::create(const std::string& name, double avgProductionTimeMin, double yieldRate) {
    const RepositoryResult validation = validateFields(avgProductionTimeMin, yieldRate);
    if (!validation.success) {
        return validation;
    }

    Sample sample;
    sample.id = nextId_++;
    sample.name = name;
    sample.avgProductionTimeMin = avgProductionTimeMin;
    sample.yieldRate = yieldRate;
    sample.stock = 0;
    cache_.push_back(sample);
    persist();
    return {true, "시료가 등록되었습니다. (ID: " + std::to_string(sample.id) + ")"};
}

RepositoryResult SampleRepository::update(int id, const SampleUpdate& patch) {
    auto it = std::find_if(cache_.begin(), cache_.end(), [id](const Sample& s) { return s.id == id; });
    if (it == cache_.end()) {
        return {false, "존재하지 않는 시료 ID입니다."};
    }

    const double newAvgTime = patch.avgProductionTimeMin.value_or(it->avgProductionTimeMin);
    const double newYield = patch.yieldRate.value_or(it->yieldRate);
    const RepositoryResult validation = validateFields(newAvgTime, newYield);
    if (!validation.success) {
        return validation;
    }

    if (patch.name) {
        it->name = *patch.name;
    }
    it->avgProductionTimeMin = newAvgTime;
    it->yieldRate = newYield;
    persist();
    return {true, "시료 정보가 수정되었습니다."};
}

RepositoryResult SampleRepository::remove(int id) {
    auto it = std::find_if(cache_.begin(), cache_.end(), [id](const Sample& s) { return s.id == id; });
    if (it == cache_.end()) {
        return {false, "존재하지 않는 시료 ID입니다."};
    }
    cache_.erase(it);
    persist();
    return {true, "시료가 삭제되었습니다."};
}

RepositoryResult SampleRepository::adjustStock(int id, int delta) {
    auto it = std::find_if(cache_.begin(), cache_.end(), [id](const Sample& s) { return s.id == id; });
    if (it == cache_.end()) {
        return {false, "존재하지 않는 시료 ID입니다."};
    }
    if (it->stock + delta < 0) {
        return {false, "재고가 음수가 될 수 없습니다."};
    }
    it->stock += delta;
    persist();
    return {true, "재고가 갱신되었습니다."};
}
