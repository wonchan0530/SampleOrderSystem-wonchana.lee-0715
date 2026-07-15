#include "Console/MonitoringMenu.h"

#include <sstream>

namespace {

std::string toString(StockLevel level) {
    switch (level) {
        case StockLevel::Depleted:
            return "고갈";
        case StockLevel::Insufficient:
            return "부족";
        case StockLevel::Sufficient:
            return "여유";
    }
    return "알수없음";
}

}  // namespace

MonitoringMenu::MonitoringMenu(MonitoringController& controller, ConsoleIO& io)
    : controller_(controller), io_(io) {}

void MonitoringMenu::handleOrderCounts() {
    const auto counts = controller_.countOrdersByStatus();
    std::ostringstream line;
    line << "RESERVED:" << counts.reserved << "건 PRODUCING:" << counts.producing
         << "건 CONFIRMED:" << counts.confirmed << "건 RELEASE:" << counts.release << "건";
    io_.println(line.str());
}

void MonitoringMenu::handleStockLevels() {
    const auto statuses = controller_.stockStatusBySample();
    if (statuses.empty()) {
        io_.println("등록된 시료가 없습니다.");
        return;
    }
    for (const auto& status : statuses) {
        std::ostringstream line;
        line << "ID:" << status.sample.id << " 이름:" << status.sample.name << " 재고:" << status.sample.stock
             << " 대기수량:" << status.pendingQuantity << " 상태:" << toString(status.level);
        io_.println(line.str());
    }
}

void MonitoringMenu::run() {
    try {
        while (true) {
            io_.println("--- 모니터링 ---");
            io_.println("1. 주문량 확인");
            io_.println("2. 재고량 확인");
            io_.println("0. 뒤로");
            const int choice = io_.readInt("선택 > ");
            if (choice == 1) {
                handleOrderCounts();
            } else if (choice == 2) {
                handleStockLevels();
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
