#include "testing.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "Repository/OrderRepository.h"
#include "Repository/SampleRepository.h"

namespace {

std::filesystem::path testFile(const std::string& name) {
    return std::filesystem::path("test_data") / name;
}

}  // namespace

TEST(orderRepository_createSucceedsForRegisteredSample) {
    const auto samplePath = testFile("order_repo_samples_create.json");
    const auto orderPath = testFile("order_repo_orders_create.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    const auto result = orderRepo.create(sampleId, "Acme Labs", 5);
    EXPECT_TRUE(result.success);

    const auto all = orderRepo.findAll();
    EXPECT_EQ(all.size(), static_cast<size_t>(1));
    EXPECT_TRUE(all[0].status == OrderStatus::RESERVED);
    EXPECT_EQ(all[0].quantity, 5);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}

TEST(orderRepository_createRejectsUnknownSampleId) {
    const auto samplePath = testFile("order_repo_samples_unknown.json");
    const auto orderPath = testFile("order_repo_orders_unknown.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    OrderRepository orderRepo(orderPath, sampleRepo);

    const auto result = orderRepo.create(9999, "Acme Labs", 5);
    EXPECT_TRUE(!result.success);
    EXPECT_TRUE(orderRepo.findAll().empty());

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}

TEST(orderRepository_createRejectsNonPositiveQuantity) {
    const auto samplePath = testFile("order_repo_samples_qty.json");
    const auto orderPath = testFile("order_repo_orders_qty.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    EXPECT_TRUE(!orderRepo.create(sampleId, "Acme Labs", 0).success);
    EXPECT_TRUE(!orderRepo.create(sampleId, "Acme Labs", -3).success);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}

TEST(orderRepository_findBySampleIdFiltersCorrectly) {
    const auto samplePath = testFile("order_repo_samples_filter.json");
    const auto orderPath = testFile("order_repo_orders_filter.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    sampleRepo.create("WaferB", 12.0, 0.85);
    const int sampleAId = sampleRepo.findAll()[0].id;
    const int sampleBId = sampleRepo.findAll()[1].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    orderRepo.create(sampleAId, "Cust1", 3);
    orderRepo.create(sampleBId, "Cust2", 4);
    orderRepo.create(sampleAId, "Cust3", 2);

    const auto forA = orderRepo.findBySampleId(sampleAId);
    EXPECT_EQ(forA.size(), static_cast<size_t>(2));

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}

TEST(orderRepository_updateAppliesPartialPatchWhileReserved) {
    const auto samplePath = testFile("order_repo_samples_update.json");
    const auto orderPath = testFile("order_repo_orders_update.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    orderRepo.create(sampleId, "Cust1", 3);
    const int orderId = orderRepo.findAll()[0].orderId;

    OrderUpdate patch;
    patch.quantity = 7;
    const auto result = orderRepo.update(orderId, patch);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(orderRepo.findById(orderId)->quantity, 7);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}

TEST(orderRepository_updateRejectsNonReservedOrder) {
    const auto samplePath = testFile("order_repo_samples_nonreserved.json");
    const auto orderPath = testFile("order_repo_orders_nonreserved.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;

    // Phase 1 does not yet implement approve/reject transitions (Phase 3+),
    // so a non-RESERVED order is simulated by writing the file directly.
    nlohmann::json orders = nlohmann::json::array();
    orders.push_back({
        {"orderId", 1},
        {"sampleId", sampleId},
        {"customerName", "Cust1"},
        {"quantity", 5},
        {"status", "CONFIRMED"},
    });
    std::filesystem::create_directories(orderPath.parent_path());
    {
        std::ofstream out(orderPath);
        out << orders.dump(2);
    }

    OrderRepository orderRepo(orderPath, sampleRepo);
    OrderUpdate patch;
    patch.quantity = 10;
    const auto result = orderRepo.update(1, patch);
    EXPECT_TRUE(!result.success);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}

TEST(orderRepository_removeDeletesOrder) {
    const auto samplePath = testFile("order_repo_samples_remove.json");
    const auto orderPath = testFile("order_repo_orders_remove.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    orderRepo.create(sampleId, "Cust1", 3);
    const int orderId = orderRepo.findAll()[0].orderId;

    EXPECT_TRUE(orderRepo.remove(orderId).success);
    EXPECT_TRUE(orderRepo.findAll().empty());
    EXPECT_TRUE(!orderRepo.remove(orderId).success);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}

TEST(orderRepository_dataPersistsAcrossInstances) {
    const auto samplePath = testFile("order_repo_samples_persistence.json");
    const auto orderPath = testFile("order_repo_orders_persistence.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;

    {
        OrderRepository orderRepo(orderPath, sampleRepo);
        orderRepo.create(sampleId, "Cust1", 3);
    }

    {
        OrderRepository reopened(orderPath, sampleRepo);
        const auto all = reopened.findAll();
        EXPECT_EQ(all.size(), static_cast<size_t>(1));
        EXPECT_EQ(all[0].customerName, "Cust1");
    }

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}

TEST(orderRepository_markReleasedOnlyAllowedFromConfirmed) {
    const auto samplePath = testFile("order_repo_samples_release.json");
    const auto orderPath = testFile("order_repo_orders_release.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    orderRepo.create(sampleId, "Cust1", 3);
    const int orderId = orderRepo.findAll()[0].orderId;

    // RESERVED 상태에서는 출고 거부
    EXPECT_TRUE(!orderRepo.markReleased(orderId).success);

    orderRepo.markConfirmed(orderId);
    const auto releaseResult = orderRepo.markReleased(orderId);
    EXPECT_TRUE(releaseResult.success);
    EXPECT_TRUE(orderRepo.findById(orderId)->status == OrderStatus::RELEASE);

    // 이미 RELEASE된 주문 재출고 거부
    EXPECT_TRUE(!orderRepo.markReleased(orderId).success);

    // 존재하지 않는 주문 ID
    EXPECT_TRUE(!orderRepo.markReleased(9999).success);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}
