#include "Console/ShippingMenu.h"

#include <sstream>
#include <vector>

#include "Console/MenuRunner.h"

namespace {

void printOrderList(ConsoleIO& io, const std::vector<Order>& orders) {
    if (orders.empty()) {
        io.println("출고 대상 주문이 없습니다.");
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

ShippingMenu::ShippingMenu(OrderController& controller, ConsoleIO& io) : controller_(controller), io_(io) {}

void ShippingMenu::handleListConfirmed() {
    printOrderList(io_, controller_.listConfirmed());
}

void ShippingMenu::handleRelease() {
    const int orderId = io_.readInt("출고할 주문 ID > ");
    io_.printResult(controller_.release(orderId));
}

void ShippingMenu::run() {
    runMenu(io_, "출고처리",
            {
                {1, "출고 대상 주문 목록", [this] { handleListConfirmed(); }},
                {2, "출고 실행", [this] { handleRelease(); }},
            });
}
