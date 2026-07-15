#include "testing.h"

#include <filesystem>

#include "Controller/MonitoringController.h"
#include "Controller/OrderController.h"
#include "Repository/OrderRepository.h"
#include "Repository/ProductionQueueRepository.h"
#include "Repository/SampleRepository.h"

namespace {

std::filesystem::path testFile(const std::string& name) {
    return std::filesystem::path("test_data") / name;
}

}  // namespace

TEST(monitoringController_countOrdersByStatusExcludesRejected) {
    const auto samplePath = testFile("monitoring_samples_counts.json");
    const auto orderPath = testFile("monitoring_orders_counts.json");
    const auto queuePath = testFile("monitoring_queue_counts.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 1.0, 1.0);
    const int sampleId = sampleRepo.findAll()[0].id;
    sampleRepo.adjustStock(sampleId, 100);

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController orderController(orderRepo, sampleRepo, queue);
    MonitoringController monitoring(sampleRepo, orderRepo);

    orderRepo.create(sampleId, "CustReserved", 1);  // stays RESERVED

    orderRepo.create(sampleId, "CustRejected", 1);
    orderController.reject(orderRepo.findAll()[1].orderId);  // REJECTED

    orderRepo.create(sampleId, "CustConfirmed", 1);
    orderController.approve(orderRepo.findAll()[2].orderId);  // sufficient stock -> CONFIRMED

    orderRepo.create(sampleId, "CustConfirmed2", 1);
    orderController.approve(orderRepo.findAll()[3].orderId);
    orderController.release(orderRepo.findAll()[3].orderId);  // RELEASE

    const auto counts = monitoring.countOrdersByStatus();
    EXPECT_EQ(counts.reserved, 1);
    EXPECT_EQ(counts.confirmed, 1);
    EXPECT_EQ(counts.release, 1);
    EXPECT_EQ(counts.producing, 0);
    // rejected order must not appear in any bucket
    EXPECT_EQ(counts.reserved + counts.producing + counts.confirmed + counts.release, 3);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(monitoringController_stockLevelDepletedWhenStockIsZero) {
    const auto samplePath = testFile("monitoring_samples_depleted.json");
    const auto orderPath = testFile("monitoring_orders_depleted.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 1.0, 1.0);  // stock stays 0

    OrderRepository orderRepo(orderPath, sampleRepo);
    MonitoringController monitoring(sampleRepo, orderRepo);

    const auto statuses = monitoring.stockStatusBySample();
    EXPECT_EQ(statuses.size(), static_cast<size_t>(1));
    EXPECT_TRUE(statuses[0].level == StockLevel::Depleted);
    EXPECT_EQ(statuses[0].pendingQuantity, 0);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}

TEST(monitoringController_stockLevelInsufficientWhenBelowPending) {
    const auto samplePath = testFile("monitoring_samples_insufficient.json");
    const auto orderPath = testFile("monitoring_orders_insufficient.json");
    const auto queuePath = testFile("monitoring_queue_insufficient.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 1.0, 1.0);
    const int sampleId = sampleRepo.findAll()[0].id;
    sampleRepo.adjustStock(sampleId, 3);

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController orderController(orderRepo, sampleRepo, queue);
    MonitoringController monitoring(sampleRepo, orderRepo);

    orderRepo.create(sampleId, "Cust", 5);  // RESERVED, quantity 5 > stock 3

    const auto statuses = monitoring.stockStatusBySample();
    EXPECT_EQ(statuses[0].pendingQuantity, 5);
    EXPECT_TRUE(statuses[0].level == StockLevel::Insufficient);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(monitoringController_stockLevelSufficientWhenStockCoversPending) {
    const auto samplePath = testFile("monitoring_samples_sufficient.json");
    const auto orderPath = testFile("monitoring_orders_sufficient.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 1.0, 1.0);
    const int sampleId = sampleRepo.findAll()[0].id;
    sampleRepo.adjustStock(sampleId, 10);

    OrderRepository orderRepo(orderPath, sampleRepo);
    MonitoringController monitoring(sampleRepo, orderRepo);
    orderRepo.create(sampleId, "Cust", 5);  // RESERVED, quantity 5 <= stock 10

    const auto statuses = monitoring.stockStatusBySample();
    EXPECT_TRUE(statuses[0].level == StockLevel::Sufficient);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}

TEST(monitoringController_pendingQuantityExcludesReleasedAndRejectedOrders) {
    const auto samplePath = testFile("monitoring_samples_pending.json");
    const auto orderPath = testFile("monitoring_orders_pending.json");
    const auto queuePath = testFile("monitoring_queue_pending.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 1.0, 1.0);
    const int sampleId = sampleRepo.findAll()[0].id;
    sampleRepo.adjustStock(sampleId, 50);

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController orderController(orderRepo, sampleRepo, queue);
    MonitoringController monitoring(sampleRepo, orderRepo);

    orderRepo.create(sampleId, "CustA", 10);
    orderController.approve(orderRepo.findAll()[0].orderId);
    orderController.release(orderRepo.findAll()[0].orderId);  // RELEASE, must not count

    orderRepo.create(sampleId, "CustB", 20);
    orderController.reject(orderRepo.findAll()[1].orderId);  // REJECTED, must not count

    orderRepo.create(sampleId, "CustC", 7);  // RESERVED, must count

    const auto statuses = monitoring.stockStatusBySample();
    EXPECT_EQ(statuses[0].pendingQuantity, 7);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(monitoringController_emptyRepositoriesReturnEmptyResults) {
    const auto samplePath = testFile("monitoring_samples_empty.json");
    const auto orderPath = testFile("monitoring_orders_empty.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    OrderRepository orderRepo(orderPath, sampleRepo);
    MonitoringController monitoring(sampleRepo, orderRepo);

    const auto counts = monitoring.countOrdersByStatus();
    EXPECT_EQ(counts.reserved + counts.producing + counts.confirmed + counts.release, 0);
    EXPECT_TRUE(monitoring.stockStatusBySample().empty());

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}
