#include "testing.h"

#include <chrono>
#include <filesystem>

#include "Controller/OrderController.h"
#include "Controller/ProductionController.h"
#include "Repository/OrderRepository.h"
#include "Repository/ProductionQueueRepository.h"
#include "Repository/SampleRepository.h"

namespace {

std::filesystem::path testFile(const std::string& name) {
    return std::filesystem::path("test_data") / name;
}

}  // namespace

TEST(productionController_completesJobAndAppliesSurplusToStock) {
    const auto samplePath = testFile("production_controller_samples_complete.json");
    const auto orderPath = testFile("production_controller_orders_complete.json");
    const auto queuePath = testFile("production_controller_queue_complete.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 1.0, 0.6);  // avg 1 min, yield 0.6
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController orderController(orderRepo, sampleRepo, queue);
    ProductionController productionController(queue, sampleRepo, orderRepo);

    const auto start = std::chrono::system_clock::now();
    orderRepo.create(sampleId, "Acme", 60);  // stock 0 -> shortfall 60
    const int orderId = orderRepo.findAll()[0].orderId;
    orderController.approve(orderId, start);

    const auto job = queue.findAll()[0];
    EXPECT_EQ(job.shortfall, 60);
    EXPECT_EQ(job.actualProductionQty, 100);  // ceil(60 / 0.6)
    EXPECT_TRUE(job.totalProductionTimeMin == 100.0);

    const int completedBefore = productionController.processCompletions(start + std::chrono::minutes(50));
    EXPECT_EQ(completedBefore, 0);
    EXPECT_TRUE(orderRepo.findById(orderId)->status == OrderStatus::PRODUCING);

    const int completedAfter = productionController.processCompletions(start + std::chrono::minutes(100));
    EXPECT_EQ(completedAfter, 1);
    EXPECT_TRUE(orderRepo.findById(orderId)->status == OrderStatus::CONFIRMED);
    EXPECT_EQ(sampleRepo.findById(sampleId)->stock, 40);  // 100 - 60 surplus
    EXPECT_TRUE(queue.empty());

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(productionController_completesExactlyAtBoundary) {
    const auto samplePath = testFile("production_controller_samples_boundary.json");
    const auto orderPath = testFile("production_controller_orders_boundary.json");
    const auto queuePath = testFile("production_controller_queue_boundary.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 2.0, 1.0);
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController orderController(orderRepo, sampleRepo, queue);
    ProductionController productionController(queue, sampleRepo, orderRepo);

    const auto start = std::chrono::system_clock::now();
    orderRepo.create(sampleId, "Acme", 5);
    const int orderId = orderRepo.findAll()[0].orderId;
    orderController.approve(orderId, start);  // shortfall 5, actualQty 5, totalTime 10 min

    const int completed = productionController.processCompletions(start + std::chrono::minutes(10));
    EXPECT_EQ(completed, 1);
    EXPECT_TRUE(orderRepo.findById(orderId)->status == OrderStatus::CONFIRMED);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(productionController_chainCompletesMultipleJobsInOneCall) {
    const auto samplePath = testFile("production_controller_samples_chain.json");
    const auto orderPath = testFile("production_controller_orders_chain.json");
    const auto queuePath = testFile("production_controller_queue_chain.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 1.0, 1.0);
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController orderController(orderRepo, sampleRepo, queue);
    ProductionController productionController(queue, sampleRepo, orderRepo);

    const auto start = std::chrono::system_clock::now();
    orderRepo.create(sampleId, "CustA", 5);  // job1: totalTime 5 min
    const int orderAId = orderRepo.findAll()[0].orderId;
    orderController.approve(orderAId, start);

    orderRepo.create(sampleId, "CustB", 3);  // job2: totalTime 3 min, waits
    const int orderBId = orderRepo.findAll()[1].orderId;
    orderController.approve(orderBId, start);

    EXPECT_EQ(queue.findAll().size(), static_cast<size_t>(2));
    EXPECT_TRUE(!queue.findAll()[1].startedAt.has_value());

    // job1 finishes at start+5min, job2 then runs 3 more (finishes start+8min).
    const int completed = productionController.processCompletions(start + std::chrono::minutes(10));
    EXPECT_EQ(completed, 2);
    EXPECT_TRUE(orderRepo.findById(orderAId)->status == OrderStatus::CONFIRMED);
    EXPECT_TRUE(orderRepo.findById(orderBId)->status == OrderStatus::CONFIRMED);
    EXPECT_TRUE(queue.empty());

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(productionController_restartScenarioCatchesUpElapsedTime) {
    const auto samplePath = testFile("production_controller_samples_restart.json");
    const auto orderPath = testFile("production_controller_orders_restart.json");
    const auto queuePath = testFile("production_controller_queue_restart.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    const auto start = std::chrono::system_clock::now();
    int sampleId = 0;
    int orderId = 0;
    {
        SampleRepository sampleRepo(samplePath);
        sampleRepo.create("WaferA", 1.0, 1.0);
        sampleId = sampleRepo.findAll()[0].id;

        OrderRepository orderRepo(orderPath, sampleRepo);
        ProductionQueueRepository queue(queuePath);
        OrderController orderController(orderRepo, sampleRepo, queue);

        orderRepo.create(sampleId, "Acme", 10);  // totalTime 10 min
        orderId = orderRepo.findAll()[0].orderId;
        orderController.approve(orderId, start);
    }

    // Simulate closing and reopening the app: fresh repository instances
    // reload from disk, then processCompletions catches up on elapsed time.
    {
        SampleRepository sampleRepo(samplePath);
        OrderRepository orderRepo(orderPath, sampleRepo);
        ProductionQueueRepository queue(queuePath);
        ProductionController productionController(queue, sampleRepo, orderRepo);

        const int completed = productionController.processCompletions(start + std::chrono::minutes(30));
        EXPECT_EQ(completed, 1);
        EXPECT_TRUE(orderRepo.findById(orderId)->status == OrderStatus::CONFIRMED);
        EXPECT_EQ(sampleRepo.findById(sampleId)->stock, 0);  // yield 1.0 -> no surplus
    }

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}
