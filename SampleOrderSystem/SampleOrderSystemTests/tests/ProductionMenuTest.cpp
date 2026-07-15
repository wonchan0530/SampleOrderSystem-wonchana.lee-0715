#include "testing.h"

#include <chrono>
#include <filesystem>
#include <sstream>

#include "Console/ConsoleIO.h"
#include "Console/ProductionMenu.h"
#include "Controller/OrderController.h"
#include "Controller/ProductionController.h"
#include "Repository/OrderRepository.h"
#include "Repository/ProductionQueueRepository.h"
#include "Repository/SampleRepository.h"

namespace {

std::filesystem::path testFile(const std::string& name) {
    return std::filesystem::path("test_data") / name;
}

}  // namespace

TEST(productionMenu_showsEmptyMessagesWhenQueueIsEmpty) {
    const auto samplePath = testFile("production_menu_samples_empty.json");
    const auto orderPath = testFile("production_menu_orders_empty.json");
    const auto queuePath = testFile("production_menu_queue_empty.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    ProductionController controller(queue, sampleRepo, orderRepo);

    std::istringstream in("1\n2\n0\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    ProductionMenu menu(controller, io);
    menu.run();

    EXPECT_TRUE(out.str().find("현재 생산 중인 시료가 없습니다.") != std::string::npos);
    EXPECT_TRUE(out.str().find("대기 중인 생산 작업이 없습니다.") != std::string::npos);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(productionMenu_showsCurrentJobAndWaitingQueue) {
    const auto samplePath = testFile("production_menu_samples_active.json");
    const auto orderPath = testFile("production_menu_orders_active.json");
    const auto queuePath = testFile("production_menu_queue_active.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 1.0, 1.0);
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController orderController(orderRepo, sampleRepo, queue);
    ProductionController controller(queue, sampleRepo, orderRepo);

    const auto now = std::chrono::system_clock::now();
    orderRepo.create(sampleId, "CustA", 5);
    orderController.approve(orderRepo.findAll()[0].orderId, now);
    orderRepo.create(sampleId, "CustB", 3);
    orderController.approve(orderRepo.findAll()[1].orderId, now);

    std::istringstream in("1\n2\n0\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    ProductionMenu menu(controller, io);
    menu.run();

    EXPECT_TRUE(out.str().find("주문ID:1") != std::string::npos);
    EXPECT_TRUE(out.str().find("대기순번:1 주문ID:2") != std::string::npos);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(productionMenu_eofDuringMenuReturnsWithoutThrowing) {
    const auto samplePath = testFile("production_menu_samples_eof.json");
    const auto orderPath = testFile("production_menu_orders_eof.json");
    const auto queuePath = testFile("production_menu_queue_eof.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    ProductionController controller(queue, sampleRepo, orderRepo);

    std::istringstream in("1\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    ProductionMenu menu(controller, io);

    menu.run();
    EXPECT_TRUE(true);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}
