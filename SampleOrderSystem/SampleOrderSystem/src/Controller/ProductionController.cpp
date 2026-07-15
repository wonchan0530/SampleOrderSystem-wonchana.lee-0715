#include "Controller/ProductionController.h"

namespace {

std::chrono::system_clock::time_point addMinutes(std::chrono::system_clock::time_point start, double minutes) {
    const auto durationToAdd =
        std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::duration<double>(minutes * 60.0));
    return start + durationToAdd;
}

}  // namespace

ProductionController::ProductionController(ProductionQueueRepository& queue, SampleRepository& sampleRepo,
                                            OrderRepository& orderRepo)
    : queue_(queue), sampleRepo_(sampleRepo), orderRepo_(orderRepo) {}

int ProductionController::processCompletions(std::chrono::system_clock::time_point now) {
    int completedCount = 0;

    while (!queue_.empty()) {
        const ProductionJob front = queue_.findAll().front();
        if (!front.startedAt) {
            break;  // defensive: the queue front should always have been started
        }

        const auto completionTime = addMinutes(*front.startedAt, front.totalProductionTimeMin);
        if (now < completionTime) {
            break;
        }

        const int surplus = front.actualProductionQty - front.shortfall;
        if (surplus > 0) {
            sampleRepo_.adjustStock(front.sampleId, surplus);
        }
        orderRepo_.markConfirmed(front.orderId);
        queue_.popFront();
        ++completedCount;

        if (!queue_.empty()) {
            queue_.setFrontStartedAt(completionTime);
        }
    }

    return completedCount;
}

std::optional<ProductionJob> ProductionController::currentJob() const {
    const auto jobs = queue_.findAll();
    if (jobs.empty()) {
        return std::nullopt;
    }
    return jobs.front();
}

std::vector<ProductionJob> ProductionController::waitingQueue() const {
    auto jobs = queue_.findAll();
    if (!jobs.empty()) {
        jobs.erase(jobs.begin());
    }
    return jobs;
}
