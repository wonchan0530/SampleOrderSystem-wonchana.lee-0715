#pragma once

#include <chrono>
#include <filesystem>
#include <vector>

#include "Model/ProductionJob.h"
#include "Storage/JsonFileStore.h"

// FIFO queue of production jobs. Enqueue is policy-free -- OrderController
// computes shortfall/actualProductionQty/totalProductionTimeMin and passes
// them in. popFront/setFrontStartedAt are low-level operations consumed by
// ProductionController (Phase 4) to advance the queue.
class ProductionQueueRepository {
public:
    explicit ProductionQueueRepository(std::filesystem::path dataFile);

    std::vector<ProductionJob> findAll() const;
    bool empty() const;

    void enqueue(int orderId, int sampleId, int shortfall, int actualProductionQty, double totalProductionTimeMin,
                 std::chrono::system_clock::time_point now);

    void popFront();
    void setFrontStartedAt(std::chrono::system_clock::time_point startedAt);

private:
    JsonFileStore<ProductionJob> store_;
    std::vector<ProductionJob> cache_;

    void persist() const;
};
