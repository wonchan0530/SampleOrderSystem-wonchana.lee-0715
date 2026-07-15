#include "testing.h"

#include <filesystem>
#include <sstream>

#include "Console/ConsoleIO.h"
#include "Console/ShippingMenu.h"
#include "Controller/OrderController.h"
#include "Repository/OrderRepository.h"
#include "Repository/ProductionQueueRepository.h"
#include "Repository/SampleRepository.h"

namespace {

std::filesystem::path testFile(const std::string& name) {
    return std::filesystem::path("test_data") / name;
}

}  // namespace

TEST(shippingMenu_listThenReleaseTransitionsOrder) {
    const auto samplePath = testFile("shipping_menu_samples_release.json");
    const auto orderPath = testFile("shipping_menu_orders_release.json");
    const auto queuePath = testFile("shipping_menu_queue_release.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;
    sampleRepo.adjustStock(sampleId, 20);

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);

    orderRepo.create(sampleId, "Acme", 5);
    const int orderId = orderRepo.findAll()[0].orderId;
    controller.approve(orderId);  // sufficient stock -> CONFIRMED

    std::ostringstream script;
    script << "1\n"          // list confirmed
           << "2\n" << orderId << "\n"  // release
           << "0\n";
    std::istringstream in(script.str());
    std::ostringstream out;
    ConsoleIO io(in, out);
    ShippingMenu menu(controller, io);
    menu.run();

    EXPECT_TRUE(out.str().find("Acme") != std::string::npos);
    EXPECT_TRUE(orderRepo.findById(orderId)->status == OrderStatus::RELEASE);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(shippingMenu_listShowsEmptyMessageWhenNothingConfirmed) {
    const auto samplePath = testFile("shipping_menu_samples_empty.json");
    const auto orderPath = testFile("shipping_menu_orders_empty.json");
    const auto queuePath = testFile("shipping_menu_queue_empty.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);

    std::istringstream in("1\n0\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    ShippingMenu menu(controller, io);
    menu.run();

    EXPECT_TRUE(out.str().find("출고 대상 주문이 없습니다.") != std::string::npos);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(shippingMenu_releaseRejectsOrderNotConfirmed) {
    const auto samplePath = testFile("shipping_menu_samples_reject.json");
    const auto orderPath = testFile("shipping_menu_orders_reject.json");
    const auto queuePath = testFile("shipping_menu_queue_reject.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);
    orderRepo.create(sampleId, "Acme", 5);  // stays RESERVED

    std::ostringstream script;
    script << "2\n1\n0\n";
    std::istringstream in(script.str());
    std::ostringstream out;
    ConsoleIO io(in, out);
    ShippingMenu menu(controller, io);
    menu.run();

    EXPECT_TRUE(orderRepo.findById(1)->status == OrderStatus::RESERVED);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(shippingMenu_eofDuringMenuReturnsWithoutThrowing) {
    const auto samplePath = testFile("shipping_menu_samples_eof.json");
    const auto orderPath = testFile("shipping_menu_orders_eof.json");
    const auto queuePath = testFile("shipping_menu_queue_eof.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);

    std::istringstream in("1\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    ShippingMenu menu(controller, io);

    menu.run();
    EXPECT_TRUE(true);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}
