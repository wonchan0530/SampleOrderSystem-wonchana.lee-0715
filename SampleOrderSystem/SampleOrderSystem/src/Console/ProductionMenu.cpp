#include "Console/ProductionMenu.h"

#include <algorithm>
#include <chrono>
#include <sstream>

#include "Console/MenuRunner.h"

ProductionMenu::ProductionMenu(ProductionController& controller, ConsoleIO& io)
    : controller_(controller), io_(io) {}

void ProductionMenu::handleCurrentStatus() {
    const auto jobOpt = controller_.currentJob();
    if (!jobOpt) {
        io_.println("현재 생산 중인 시료가 없습니다.");
        return;
    }

    const auto& job = *jobOpt;
    double elapsedMin = 0.0;
    if (job.startedAt) {
        const auto now = std::chrono::system_clock::now();
        elapsedMin = std::chrono::duration<double>(now - *job.startedAt).count() / 60.0;
    }
    double remainingMin = job.totalProductionTimeMin - elapsedMin;
    if (remainingMin < 0.0) {
        remainingMin = 0.0;
    }

    std::ostringstream line;
    line << "주문ID:" << job.orderId << " 시료ID:" << job.sampleId << " 실생산량:" << job.actualProductionQty
         << " 총생산시간(분):" << job.totalProductionTimeMin << " 경과(분):" << elapsedMin
         << " 남은시간(분):" << remainingMin;
    io_.println(line.str());
}

void ProductionMenu::handleWaitingQueue() {
    const auto waiting = controller_.waitingQueue();
    if (waiting.empty()) {
        io_.println("대기 중인 생산 작업이 없습니다.");
        return;
    }

    int order = 1;
    for (const auto& job : waiting) {
        std::ostringstream line;
        line << "대기순번:" << order++ << " 주문ID:" << job.orderId << " 시료ID:" << job.sampleId
             << " 실생산량:" << job.actualProductionQty;
        io_.println(line.str());
    }
}

void ProductionMenu::run() {
    runMenu(io_, "생산라인",
            {
                {1, "생산 현황", [this] { handleCurrentStatus(); }},
                {2, "대기 주문 확인", [this] { handleWaitingQueue(); }},
            });
}
