#include "Repository/ProductionQueueRepository.h"

ProductionQueueRepository::ProductionQueueRepository(std::filesystem::path dataFile) : store_(std::move(dataFile)) {
    cache_ = store_.load();
}

void ProductionQueueRepository::persist() const {
    store_.save(cache_);
}

std::vector<ProductionJob> ProductionQueueRepository::findAll() const {
    return cache_;
}

bool ProductionQueueRepository::empty() const {
    return cache_.empty();
}

void ProductionQueueRepository::enqueue(int orderId, int sampleId, int shortfall, int actualProductionQty,
                                         double totalProductionTimeMin,
                                         std::chrono::system_clock::time_point now) {
    ProductionJob job;
    job.orderId = orderId;
    job.sampleId = sampleId;
    job.shortfall = shortfall;
    job.actualProductionQty = actualProductionQty;
    job.totalProductionTimeMin = totalProductionTimeMin;
    job.startedAt = cache_.empty() ? std::make_optional(now) : std::nullopt;

    cache_.push_back(job);
    persist();
}

void ProductionQueueRepository::popFront() {
    if (cache_.empty()) {
        return;
    }
    cache_.erase(cache_.begin());
    persist();
}

void ProductionQueueRepository::setFrontStartedAt(std::chrono::system_clock::time_point startedAt) {
    if (cache_.empty()) {
        return;
    }
    cache_.front().startedAt = startedAt;
    persist();
}
