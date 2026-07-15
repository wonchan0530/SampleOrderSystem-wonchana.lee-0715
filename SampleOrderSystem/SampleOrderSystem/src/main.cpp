#include <algorithm>
#include <string>

#include "Console/ConsoleIO.h"
#include "Console/MonitoringMenu.h"
#include "Console/OrderMenu.h"
#include "Console/ProductionMenu.h"
#include "Console/SampleMenu.h"
#include "Console/ShippingMenu.h"
#include "Controller/MonitoringController.h"
#include "Controller/OrderController.h"
#include "Controller/ProductionController.h"
#include "Repository/OrderRepository.h"
#include "Repository/ProductionQueueRepository.h"
#include "Repository/SampleRepository.h"
#include "Storage/PathUtil.h"

int main() {
    ConsoleIO io;
    SampleRepository sampleRepo(PathUtil::dataDir() / "samples.json");
    OrderRepository orderRepo(PathUtil::dataDir() / "orders.json", sampleRepo);
    ProductionQueueRepository productionQueue(PathUtil::dataDir() / "production_queue.json");
    OrderController orderController(orderRepo, sampleRepo, productionQueue);
    ProductionController productionController(productionQueue, sampleRepo, orderRepo);
    MonitoringController monitoringController(sampleRepo, orderRepo);

    SampleMenu sampleMenu(sampleRepo, io);
    OrderMenu orderMenu(orderController, io);
    MonitoringMenu monitoringMenu(monitoringController, io);
    ShippingMenu shippingMenu(orderController, io);
    ProductionMenu productionMenu(productionController, io);

    while (true) {
        // Catches up any production that finished while a menu wasn't open
        // (including while the app itself was closed) before every screen.
        productionController.processCompletions();

        const auto samples = sampleRepo.findAll();
        const auto stockStatuses = monitoringController.stockStatusBySample();
        const int needsAttention = static_cast<int>(std::count_if(
            stockStatuses.begin(), stockStatuses.end(),
            [](const SampleStockStatus& s) { return s.level != StockLevel::Sufficient; }));

        io.println("=== 반도체 시료 생산주문관리 시스템 ===");
        io.println("등록된 시료: " + std::to_string(samples.size()) + "종 (재고 주의 필요: " +
                    std::to_string(needsAttention) + "종)");
        io.println("1. 시료관리");
        io.println("2. 주문(접수/승인/거절)");
        io.println("3. 모니터링");
        io.println("4. 출고처리");
        io.println("5. 생산라인");
        io.println("0. 종료");
        try {
            const int choice = io.readInt("선택 > ");
            if (choice == 1) {
                sampleMenu.run();
            } else if (choice == 2) {
                orderMenu.run();
            } else if (choice == 3) {
                monitoringMenu.run();
            } else if (choice == 4) {
                shippingMenu.run();
            } else if (choice == 5) {
                productionMenu.run();
            } else if (choice == 0) {
                break;
            } else {
                io.println("잘못된 선택입니다.");
            }
        } catch (const EofException&) {
            break;
        }
    }
    return 0;
}
