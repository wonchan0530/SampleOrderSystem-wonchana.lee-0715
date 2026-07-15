#include "Repository/OrderRepository.h"

#include <algorithm>

OrderRepository::OrderRepository(std::filesystem::path dataFile, const SampleRepository& sampleRepository)
    : store_(std::move(dataFile)), sampleRepository_(sampleRepository) {
    cache_ = store_.load();
    recalcNextId();
}

void OrderRepository::recalcNextId() {
    int maxId = 0;
    for (const auto& o : cache_) {
        maxId = std::max(maxId, o.orderId);
    }
    nextId_ = maxId + 1;
}

void OrderRepository::persist() const {
    store_.save(cache_);
}

std::vector<Order> OrderRepository::findAll() const {
    return cache_;
}

std::optional<Order> OrderRepository::findById(int orderId) const {
    auto it = std::find_if(cache_.begin(), cache_.end(), [orderId](const Order& o) { return o.orderId == orderId; });
    if (it == cache_.end()) {
        return std::nullopt;
    }
    return *it;
}

std::vector<Order> OrderRepository::findBySampleId(int sampleId) const {
    std::vector<Order> result;
    for (const auto& o : cache_) {
        if (o.sampleId == sampleId) {
            result.push_back(o);
        }
    }
    return result;
}

RepositoryResult OrderRepository::create(int sampleId, const std::string& customerName, int quantity) {
    if (!sampleRepository_.findById(sampleId)) {
        return {false, "존재하지 않는 시료 ID입니다."};
    }
    if (quantity <= 0) {
        return {false, "주문 수량은 1 이상이어야 합니다."};
    }

    Order order;
    order.orderId = nextId_++;
    order.sampleId = sampleId;
    order.customerName = customerName;
    order.quantity = quantity;
    order.status = OrderStatus::RESERVED;
    cache_.push_back(order);
    persist();
    return {true, "주문이 접수되었습니다. (주문 ID: " + std::to_string(order.orderId) + ")"};
}

RepositoryResult OrderRepository::update(int orderId, const OrderUpdate& patch) {
    auto it = std::find_if(cache_.begin(), cache_.end(), [orderId](const Order& o) { return o.orderId == orderId; });
    if (it == cache_.end()) {
        return {false, "존재하지 않는 주문 ID입니다."};
    }
    if (it->status != OrderStatus::RESERVED) {
        return {false, "접수(RESERVED) 상태의 주문만 수정할 수 있습니다."};
    }
    if (patch.quantity && *patch.quantity <= 0) {
        return {false, "주문 수량은 1 이상이어야 합니다."};
    }

    if (patch.customerName) {
        it->customerName = *patch.customerName;
    }
    if (patch.quantity) {
        it->quantity = *patch.quantity;
    }
    persist();
    return {true, "주문 정보가 수정되었습니다."};
}

RepositoryResult OrderRepository::remove(int orderId) {
    auto it = std::find_if(cache_.begin(), cache_.end(), [orderId](const Order& o) { return o.orderId == orderId; });
    if (it == cache_.end()) {
        return {false, "존재하지 않는 주문 ID입니다."};
    }
    cache_.erase(it);
    persist();
    return {true, "주문이 삭제되었습니다."};
}
