#include "testing.h"

#include <filesystem>
#include <sstream>

#include "Console/ConsoleIO.h"
#include "Console/MonitoringMenu.h"
#include "Controller/MonitoringController.h"
#include "Controller/OrderController.h"
#include "Repository/OrderRepository.h"
#include "Repository/ProductionQueueRepository.h"
#include "Repository/SampleRepository.h"

namespace {

std::filesystem::path testFile(const std::string& name) {
    return std::filesystem::path("test_data") / name;
}

}  // namespace

TEST(monitoringMenu_showsOrderCountsAndStockLevels) {
    const auto samplePath = testFile("monitoring_menu_samples_show.json");
    const auto orderPath = testFile("monitoring_menu_orders_show.json");
    const auto queuePath = testFile("monitoring_menu_queue_show.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 1.0, 1.0);  // stock 0 -> depleted

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController orderController(orderRepo, sampleRepo, queue);
    MonitoringController controller(sampleRepo, orderRepo);

    const int sampleId = sampleRepo.findAll()[0].id;
    orderRepo.create(sampleId, "Cust", 3);  // RESERVED

    std::istringstream in("1\n2\n0\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    MonitoringMenu menu(controller, io);
    menu.run();

    EXPECT_TRUE(out.str().find("RESERVED:1건") != std::string::npos);
    EXPECT_TRUE(out.str().find("고갈") != std::string::npos);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(monitoringMenu_stockLevelsShowsEmptyMessageWhenNoSamples) {
    const auto samplePath = testFile("monitoring_menu_samples_empty.json");
    const auto orderPath = testFile("monitoring_menu_orders_empty.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    OrderRepository orderRepo(orderPath, sampleRepo);
    MonitoringController controller(sampleRepo, orderRepo);

    std::istringstream in("2\n0\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    MonitoringMenu menu(controller, io);
    menu.run();

    EXPECT_TRUE(out.str().find("등록된 시료가 없습니다.") != std::string::npos);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}

TEST(monitoringMenu_eofDuringMenuReturnsWithoutThrowing) {
    const auto samplePath = testFile("monitoring_menu_samples_eof.json");
    const auto orderPath = testFile("monitoring_menu_orders_eof.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);

    SampleRepository sampleRepo(samplePath);
    OrderRepository orderRepo(orderPath, sampleRepo);
    MonitoringController controller(sampleRepo, orderRepo);

    std::istringstream in("1\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    MonitoringMenu menu(controller, io);

    menu.run();
    EXPECT_TRUE(true);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
}
