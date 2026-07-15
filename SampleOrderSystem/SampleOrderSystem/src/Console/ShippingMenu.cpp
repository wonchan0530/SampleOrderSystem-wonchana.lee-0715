#include "Console/ShippingMenu.h"

#include <sstream>
#include <vector>

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
    try {
        while (true) {
            io_.println("--- 출고처리 ---");
            io_.println("1. 출고 대상 주문 목록");
            io_.println("2. 출고 실행");
            io_.println("0. 뒤로");
            const int choice = io_.readInt("선택 > ");
            if (choice == 1) {
                handleListConfirmed();
            } else if (choice == 2) {
                handleRelease();
            } else if (choice == 0) {
                return;
            } else {
                io_.println("잘못된 선택입니다.");
            }
        }
    } catch (const EofException&) {
        return;
    }
}
