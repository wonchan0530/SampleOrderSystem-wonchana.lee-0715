#include "Controller/MonitoringController.h"

namespace {

bool isPending(OrderStatus status) {
    return status == OrderStatus::RESERVED || status == OrderStatus::PRODUCING ||
           status == OrderStatus::CONFIRMED;
}

StockLevel classifyStockLevel(int stock, int pendingQuantity) {
    if (stock == 0) {
        return StockLevel::Depleted;
    }
    if (stock < pendingQuantity) {
        return StockLevel::Insufficient;
    }
    return StockLevel::Sufficient;
}

}  // namespace

MonitoringController::MonitoringController(const SampleRepository& sampleRepo, const OrderRepository& orderRepo)
    : sampleRepo_(sampleRepo), orderRepo_(orderRepo) {}

OrderStatusCounts MonitoringController::countOrdersByStatus() const {
    OrderStatusCounts counts;
    for (const auto& order : orderRepo_.findAll()) {
        switch (order.status) {
            case OrderStatus::RESERVED:
                ++counts.reserved;
                break;
            case OrderStatus::PRODUCING:
                ++counts.producing;
                break;
            case OrderStatus::CONFIRMED:
                ++counts.confirmed;
                break;
            case OrderStatus::RELEASE:
                ++counts.release;
                break;
            case OrderStatus::REJECTED:
                break;  // excluded from monitoring
        }
    }
    return counts;
}

std::vector<SampleStockStatus> MonitoringController::stockStatusBySample() const {
    std::vector<SampleStockStatus> result;
    for (const auto& sample : sampleRepo_.findAll()) {
        int pendingQuantity = 0;
        for (const auto& order : orderRepo_.findBySampleId(sample.id)) {
            if (isPending(order.status)) {
                pendingQuantity += order.quantity;
            }
        }
        result.push_back({sample, pendingQuantity, classifyStockLevel(sample.stock, pendingQuantity)});
    }
    return result;
}
