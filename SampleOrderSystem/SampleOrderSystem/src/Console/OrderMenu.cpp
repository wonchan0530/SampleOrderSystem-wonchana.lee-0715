#include "Console/OrderMenu.h"

#include <sstream>
#include <vector>

#include "Console/MenuRunner.h"

namespace {

void printOrderList(ConsoleIO& io, const std::vector<Order>& orders) {
    if (orders.empty()) {
        io.println("접수된 주문이 없습니다.");
        return;
    }
    for (const auto& o : orders) {
        std::ostringstream line;
        line << "주문ID:" << o.orderId << " 시료ID:" << o.sampleId << " 고객명:" << o.customerName
             << " 수량:" << o.quantity << " 상태:" << toString(o.status);
        io.println(line.str());
    }
}

}  // namespace

OrderMenu::OrderMenu(OrderController& controller, ConsoleIO& io) : controller_(controller), io_(io) {}

void OrderMenu::handlePlaceOrder() {
    const int sampleId = io_.readInt("시료 ID > ");
    std::string customerName = io_.readLine("고객명 > ");
    while (customerName.empty()) {
        io_.println("고객명은 비워둘 수 없습니다.");
        customerName = io_.readLine("고객명 > ");
    }
    const int quantity = io_.readInt("주문 수량 > ");
    io_.printResult(controller_.placeOrder(sampleId, customerName, quantity));
}

void OrderMenu::handleListReserved() {
    printOrderList(io_, controller_.listReserved());
}

void OrderMenu::handleApprove() {
    const int orderId = io_.readInt("승인할 주문 ID > ");
    io_.printResult(controller_.approve(orderId));
}

void OrderMenu::handleReject() {
    const int orderId = io_.readInt("거절할 주문 ID > ");
    io_.printResult(controller_.reject(orderId));
}

void OrderMenu::run() {
    runMenu(io_, "주문(접수/승인/거절)",
            {
                {1, "시료 주문(접수)", [this] { handlePlaceOrder(); }},
                {2, "접수된 주문 목록", [this] { handleListReserved(); }},
                {3, "주문 승인", [this] { handleApprove(); }},
                {4, "주문 거절", [this] { handleReject(); }},
            });
}
