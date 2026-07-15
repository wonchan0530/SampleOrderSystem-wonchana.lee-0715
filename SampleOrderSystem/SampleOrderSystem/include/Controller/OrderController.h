#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "Model/Order.h"
#include "Repository/OrderRepository.h"
#include "Repository/ProductionQueueRepository.h"
#include "Repository/RepositoryResult.h"
#include "Repository/SampleRepository.h"

// Owns the stock policy from PRD.md SS6.1: stock is checked and deducted
// exactly once, at approval time. OrderRepository/SampleRepository only
// enforce legal state/values; this controller decides *when* to apply them.
class OrderController {
public:
    OrderController(OrderRepository& orderRepo, SampleRepository& sampleRepo,
                     ProductionQueueRepository& productionQueue);

    RepositoryResult placeOrder(int sampleId, const std::string& customerName, int quantity);
    std::vector<Order> listReserved() const;
    std::vector<Order> listConfirmed() const;

    RepositoryResult approve(int orderId,
                             std::chrono::system_clock::time_point now = std::chrono::system_clock::now());
    RepositoryResult reject(int orderId);
    RepositoryResult release(int orderId);

private:
    OrderRepository& orderRepo_;
    SampleRepository& sampleRepo_;
    ProductionQueueRepository& productionQueue_;
};
