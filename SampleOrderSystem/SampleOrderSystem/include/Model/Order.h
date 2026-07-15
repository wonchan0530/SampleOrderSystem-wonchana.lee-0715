#pragma once

#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

enum class OrderStatus {
    RESERVED,
    REJECTED,
    PRODUCING,
    CONFIRMED,
    RELEASE,
};

inline std::string toString(OrderStatus status) {
    switch (status) {
        case OrderStatus::RESERVED:
            return "RESERVED";
        case OrderStatus::REJECTED:
            return "REJECTED";
        case OrderStatus::PRODUCING:
            return "PRODUCING";
        case OrderStatus::CONFIRMED:
            return "CONFIRMED";
        case OrderStatus::RELEASE:
            return "RELEASE";
    }
    throw std::invalid_argument("Unknown OrderStatus value");
}

inline OrderStatus orderStatusFromString(const std::string& s) {
    if (s == "RESERVED") return OrderStatus::RESERVED;
    if (s == "REJECTED") return OrderStatus::REJECTED;
    if (s == "PRODUCING") return OrderStatus::PRODUCING;
    if (s == "CONFIRMED") return OrderStatus::CONFIRMED;
    if (s == "RELEASE") return OrderStatus::RELEASE;
    throw std::invalid_argument("Unknown OrderStatus string: " + s);
}

struct Order {
    int orderId = 0;
    int sampleId = 0;
    std::string customerName;
    int quantity = 0;
    OrderStatus status = OrderStatus::RESERVED;
};

inline void to_json(nlohmann::json& j, const Order& o) {
    j = nlohmann::json{
        {"orderId", o.orderId},
        {"sampleId", o.sampleId},
        {"customerName", o.customerName},
        {"quantity", o.quantity},
        {"status", toString(o.status)},
    };
}

inline void from_json(const nlohmann::json& j, Order& o) {
    j.at("orderId").get_to(o.orderId);
    j.at("sampleId").get_to(o.sampleId);
    j.at("customerName").get_to(o.customerName);
    j.at("quantity").get_to(o.quantity);
    o.status = orderStatusFromString(j.at("status").get<std::string>());
}
