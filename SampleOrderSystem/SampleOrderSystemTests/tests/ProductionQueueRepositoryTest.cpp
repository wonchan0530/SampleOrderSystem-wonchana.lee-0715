#include "testing.h"

#include <chrono>
#include <filesystem>

#include "Repository/ProductionQueueRepository.h"

namespace {

std::filesystem::path testFile(const std::string& name) {
    return std::filesystem::path("test_data") / name;
}

}  // namespace

TEST(productionQueueRepository_enqueueOnEmptyQueueStartsImmediately) {
    const auto path = testFile("production_queue_empty_enqueue.json");
    std::filesystem::remove(path);
    ProductionQueueRepository queue(path);

    const auto now = std::chrono::system_clock::now();
    queue.enqueue(1, 10, 5, 6, 12.0, now);

    const auto all = queue.findAll();
    EXPECT_EQ(all.size(), static_cast<size_t>(1));
    EXPECT_TRUE(all[0].startedAt.has_value());

    std::filesystem::remove(path);
}

TEST(productionQueueRepository_enqueueOnNonEmptyQueueWaits) {
    const auto path = testFile("production_queue_wait_enqueue.json");
    std::filesystem::remove(path);
    ProductionQueueRepository queue(path);

    const auto now = std::chrono::system_clock::now();
    queue.enqueue(1, 10, 5, 6, 12.0, now);
    queue.enqueue(2, 10, 3, 4, 8.0, now);

    const auto all = queue.findAll();
    EXPECT_EQ(all.size(), static_cast<size_t>(2));
    EXPECT_TRUE(all[0].startedAt.has_value());
    EXPECT_TRUE(!all[1].startedAt.has_value());

    std::filesystem::remove(path);
}

TEST(productionQueueRepository_dataPersistsAcrossInstances) {
    const auto path = testFile("production_queue_persistence.json");
    std::filesystem::remove(path);

    const auto now = std::chrono::system_clock::now();
    {
        ProductionQueueRepository queue(path);
        queue.enqueue(1, 10, 5, 6, 12.0, now);
    }

    {
        ProductionQueueRepository reopened(path);
        const auto all = reopened.findAll();
        EXPECT_EQ(all.size(), static_cast<size_t>(1));
        EXPECT_TRUE(all[0].startedAt.has_value());
        const auto diffSeconds =
            std::chrono::duration_cast<std::chrono::seconds>(*all[0].startedAt - now).count();
        EXPECT_TRUE(diffSeconds > -2 && diffSeconds < 2);  // stored at second precision
    }

    std::filesystem::remove(path);
}

TEST(productionQueueRepository_popFrontAndSetFrontStartedAt) {
    const auto path = testFile("production_queue_pop.json");
    std::filesystem::remove(path);
    ProductionQueueRepository queue(path);

    const auto now = std::chrono::system_clock::now();
    queue.enqueue(1, 10, 5, 6, 12.0, now);
    queue.enqueue(2, 10, 3, 4, 8.0, now);

    queue.popFront();
    EXPECT_EQ(queue.findAll().size(), static_cast<size_t>(1));
    EXPECT_TRUE(!queue.findAll()[0].startedAt.has_value());

    const auto chainedStart = now + std::chrono::minutes(12);
    queue.setFrontStartedAt(chainedStart);
    EXPECT_TRUE(queue.findAll()[0].startedAt.has_value());

    std::filesystem::remove(path);
}
