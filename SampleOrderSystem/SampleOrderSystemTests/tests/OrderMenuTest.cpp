#include "testing.h"

#include <filesystem>
#include <sstream>

#include "Console/ConsoleIO.h"
#include "Console/OrderMenu.h"
#include "Controller/OrderController.h"
#include "Repository/OrderRepository.h"
#include "Repository/ProductionQueueRepository.h"
#include "Repository/SampleRepository.h"

namespace {

std::filesystem::path testFile(const std::string& name) {
    return std::filesystem::path("test_data") / name;
}

}  // namespace

TEST(orderMenu_placeOrderThenListShowsReservedOrder) {
    const auto samplePath = testFile("order_menu_samples_place.json");
    const auto orderPath = testFile("order_menu_orders_place.json");
    const auto queuePath = testFile("order_menu_queue_place.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);

    std::ostringstream script;
    script << "1\n" << sampleId << "\nAcme\n5\n2\n0\n";
    std::istringstream in(script.str());
    std::ostringstream out;
    ConsoleIO io(in, out);
    OrderMenu menu(controller, io);
    menu.run();

    EXPECT_TRUE(out.str().find("Acme") != std::string::npos);
    EXPECT_EQ(orderRepo.findAll().size(), static_cast<size_t>(1));

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(orderMenu_approveConfirmsOrderWhenStockSufficient) {
    const auto samplePath = testFile("order_menu_samples_approve.json");
    const auto orderPath = testFile("order_menu_orders_approve.json");
    const auto queuePath = testFile("order_menu_queue_approve.json");
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

    std::ostringstream script;
    script << "1\n" << sampleId << "\nAcme\n5\n"  // place order -> becomes orderId 1
           << "3\n1\n"                              // approve order 1
           << "0\n";
    std::istringstream in(script.str());
    std::ostringstream out;
    ConsoleIO io(in, out);
    OrderMenu menu(controller, io);
    menu.run();

    EXPECT_TRUE(orderRepo.findById(1)->status == OrderStatus::CONFIRMED);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(orderMenu_rejectTransitionsOrderToRejected) {
    const auto samplePath = testFile("order_menu_samples_reject.json");
    const auto orderPath = testFile("order_menu_orders_reject.json");
    const auto queuePath = testFile("order_menu_queue_reject.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    sampleRepo.create("WaferA", 10.0, 0.9);
    const int sampleId = sampleRepo.findAll()[0].id;

    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);

    std::ostringstream script;
    script << "1\n" << sampleId << "\nAcme\n5\n"  // place order -> orderId 1
           << "4\n1\n"                              // reject order 1
           << "0\n";
    std::istringstream in(script.str());
    std::ostringstream out;
    ConsoleIO io(in, out);
    OrderMenu menu(controller, io);
    menu.run();

    EXPECT_TRUE(orderRepo.findById(1)->status == OrderStatus::REJECTED);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}

TEST(orderMenu_eofDuringMenuReturnsWithoutThrowing) {
    const auto samplePath = testFile("order_menu_samples_eof.json");
    const auto orderPath = testFile("order_menu_orders_eof.json");
    const auto queuePath = testFile("order_menu_queue_eof.json");
    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);

    SampleRepository sampleRepo(samplePath);
    OrderRepository orderRepo(orderPath, sampleRepo);
    ProductionQueueRepository queue(queuePath);
    OrderController controller(orderRepo, sampleRepo, queue);

    std::istringstream in("2\n");  // list, then EOF at next choice prompt
    std::ostringstream out;
    ConsoleIO io(in, out);
    OrderMenu menu(controller, io);

    menu.run();
    EXPECT_TRUE(true);

    std::filesystem::remove(samplePath);
    std::filesystem::remove(orderPath);
    std::filesystem::remove(queuePath);
}
