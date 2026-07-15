#include "Controller/OrderController.h"

#include <cmath>

OrderController::OrderController(OrderRepository& orderRepo, SampleRepository& sampleRepo,
                                  ProductionQueueRepository& productionQueue)
    : orderRepo_(orderRepo), sampleRepo_(sampleRepo), productionQueue_(productionQueue) {}

RepositoryResult OrderController::placeOrder(int sampleId, const std::string& customerName, int quantity) {
    return orderRepo_.create(sampleId, customerName, quantity);
}

std::vector<Order> OrderController::listReserved() const {
    std::vector<Order> result;
    for (const auto& order : orderRepo_.findAll()) {
        if (order.status == OrderStatus::RESERVED) {
            result.push_back(order);
        }
    }
    return result;
}

RepositoryResult OrderController::approve(int orderId, std::chrono::system_clock::time_point now) {
    const auto orderOpt = orderRepo_.findById(orderId);
    if (!orderOpt) {
        return {false, "존재하지 않는 주문 ID입니다."};
    }
    if (orderOpt->status != OrderStatus::RESERVED) {
        return {false, "RESERVED 상태의 주문만 승인할 수 있습니다."};
    }

    const auto sampleOpt = sampleRepo_.findById(orderOpt->sampleId);
    if (!sampleOpt) {
        return {false, "주문이 참조하는 시료를 찾을 수 없습니다."};
    }

    const int stock = sampleOpt->stock;
    const int quantity = orderOpt->quantity;

    if (stock >= quantity) {
        sampleRepo_.adjustStock(sampleOpt->id, -quantity);
        return orderRepo_.markConfirmed(orderId);
    }

    const int shortfall = quantity - stock;
    if (stock > 0) {
        sampleRepo_.adjustStock(sampleOpt->id, -stock);
    }
    const int actualProductionQty = static_cast<int>(std::ceil(shortfall / sampleOpt->yieldRate));
    const double totalProductionTimeMin = sampleOpt->avgProductionTimeMin * actualProductionQty;
    productionQueue_.enqueue(orderId, sampleOpt->id, shortfall, actualProductionQty, totalProductionTimeMin, now);

    return orderRepo_.markProducing(orderId);
}

RepositoryResult OrderController::reject(int orderId) {
    return orderRepo_.markRejected(orderId);
}
