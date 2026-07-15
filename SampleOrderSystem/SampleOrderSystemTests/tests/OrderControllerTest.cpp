#include "testing.h"

#include <chrono>
#include <filesystem>

#include "Controller/OrderController.h"
#include "Repository/OrderRepository.h"
#include "Repository/ProductionQueueRepository.h"
#include "Repository/SampleRepository.h"

namespace {

std::filesystem::path testFile(const std::string& name) {
    return std::filesystem::path("test_data") / name;
}

}  // namespace

TEST(orderController_approveConfirmsImmediatelyWhenStockSufficient) {
    const auto samplePath = testFile("order_controller_samples_sufficient.json");
    const auto orderPath = testFile("order_controller_orders_sufficient.json");
    const auto queuePath = testFile("order_controller_queue_sufficient.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;
    sampleRepo.adjustStock(sampleId, 20);

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);

    orderRepo.create(sampleId, "Acme", 15);
    const int orderId = orderRepo.findAll()[0].orderId;

    const auto result = controller.approve(orderId);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(orderRepo.findById(orderId)->status == OrderStatus::CONFIRMED);
    EXPECT_EQ(sampleRepo.findById(sampleId)->stock, 5);
    EXPECT_TRUE(queue.empty());

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(orderController_approveEnqueuesShortfallWhenStockInsufficient) {
    const auto samplePath = testFile("order_controller_samples_shortfall.json");
    const auto orderPath = testFile("order_controller_orders_shortfall.json");
    const auto queuePath = testFile("order_controller_queue_shortfall.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.6);
    const int sampleId = sampleRepo.findAll()[0].id;
    sampleRepo.adjustStock(sampleId, 40);

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);

    orderRepo.create(sampleId, "Acme", 100);  // shortfall = 100 - 40 = 60
    const int orderId = orderRepo.findAll()[0].orderId;

    const auto result = controller.approve(orderId, std::chrono::system_clock::now());
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(orderRepo.findById(orderId)->status == OrderStatus::PRODUCING);
    EXPECT_EQ(sampleRepo.findById(sampleId)->stock, 0);

    const auto jobs = queue.findAll();
    EXPECT_EQ(jobs.size(), static_cast<size_t>(1));
    EXPECT_EQ(jobs[0].shortfall, 60);
    EXPECT_EQ(jobs[0].actualProductionQty, 100);  // ceil(60 / 0.6) = 100
    EXPECT_TRUE(jobs[0].totalProductionTimeMin == 10.0 * 100);
    EXPECT_TRUE(jobs[0].startedAt.has_value());

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(orderController_subsequentOrderForSameSampleDoesNotOverlapEarlierShare) {
    const auto samplePath = testFile("order_controller_samples_isolation.json");
    const auto orderPath = testFile("order_controller_orders_isolation.json");
    const auto queuePath = testFile("order_controller_queue_isolation.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;
    sampleRepo.adjustStock(sampleId, 10);

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);

    orderRepo.create(sampleId, "CustA", 15);
    const int orderAId = orderRepo.findAll()[0].orderId;
    controller.approve(orderAId);

    EXPECT_EQ(sampleRepo.findById(sampleId)->stock, 0);
    EXPECT_EQ(queue.findAll().size(), static_cast<size_t>(1));
    EXPECT_EQ(queue.findAll()[0].shortfall, 5);

    orderRepo.create(sampleId, "CustB", 8);
    const int orderBId = orderRepo.findAll()[1].orderId;
    controller.approve(orderBId);

    EXPECT_EQ(sampleRepo.findById(sampleId)->stock, 0);
    const auto jobs = queue.findAll();
    EXPECT_EQ(jobs.size(), static_cast<size_t>(2));
    EXPECT_EQ(jobs[1].shortfall, 8);  // stock was already 0, so B's shortfall is its full quantity

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(orderController_approveRejectsAlreadyDecidedOrder) {
    const auto samplePath = testFile("order_controller_samples_nonreserved.json");
    const auto orderPath = testFile("order_controller_orders_nonreserved.json");
    const auto queuePath = testFile("order_controller_queue_nonreserved.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;
    sampleRepo.adjustStock(sampleId, 20);

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);

    orderRepo.create(sampleId, "Acme", 5);
    const int orderId = orderRepo.findAll()[0].orderId;
    controller.approve(orderId);

    const auto secondAttempt = controller.approve(orderId);
    EXPECT_TRUE(!secondAttempt.success);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(orderController_rejectTransitionsToRejectedAndBlocksFurtherApproval) {
    const auto samplePath = testFile("order_controller_samples_reject.json");
    const auto orderPath = testFile("order_controller_orders_reject.json");
    const auto queuePath = testFile("order_controller_queue_reject.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);

    orderRepo.create(sampleId, "Acme", 5);
    const int orderId = orderRepo.findAll()[0].orderId;

    const auto rejectResult = controller.reject(orderId);
    EXPECT_TRUE(rejectResult.success);
    EXPECT_TRUE(orderRepo.findById(orderId)->status == OrderStatus::REJECTED);

    const auto approveAfterReject = controller.approve(orderId);
    EXPECT_TRUE(!approveAfterReject.success);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(orderController_approveAndRejectRejectUnknownOrderId) {
    const auto samplePath = testFile("order_controller_samples_unknown.json");
    const auto orderPath = testFile("order_controller_orders_unknown.json");
    const auto queuePath = testFile("order_controller_queue_unknown.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);

    EXPECT_TRUE(!controller.approve(9999).success);
    EXPECT_TRUE(!controller.reject(9999).success);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}
