#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "Model/ProductionJob.h"
#include "Repository/OrderRepository.h"
#include "Repository/ProductionQueueRepository.h"
#include "Repository/SampleRepository.h"

// Wall-clock completion judgment (PRD.md SS6.3 / docs/06-생산라인.md SS3).
// processCompletions is called once per main-menu loop iteration (see
// design/phase4-design.md SS4) so it covers both "on menu entry" and
// "on restart" in a single call site.
class ProductionController {
public:
    ProductionController(ProductionQueueRepository& queue, SampleRepository& sampleRepo, OrderRepository& orderRepo);

    // Completes every queue-front job whose start + totalProductionTimeMin
    // has already passed `now`, chaining through the queue. Returns how
    // many jobs were completed.
    int processCompletions(std::chrono::system_clock::time_point now = std::chrono::system_clock::now());

    std::optional<ProductionJob> currentJob() const;    // queue front (in progress), if any
    std::vector<ProductionJob> waitingQueue() const;    // everything but the front, FIFO order

private:
    ProductionQueueRepository& queue_;
    SampleRepository& sampleRepo_;
    OrderRepository& orderRepo_;
};
