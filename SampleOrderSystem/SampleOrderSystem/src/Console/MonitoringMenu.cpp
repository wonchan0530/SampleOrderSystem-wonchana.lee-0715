#include "Console/MonitoringMenu.h"

#include <sstream>

#include "Console/MenuRunner.h"

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
    runMenu(io_, "모니터링",
            {
                {1, "주문량 확인", [this] { handleOrderCounts(); }},
                {2, "재고량 확인", [this] { handleStockLevels(); }},
            });
}
