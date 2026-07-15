#pragma once

#include "Console/ConsoleIO.h"
#include "Controller/OrderController.h"

class OrderMenu {
public:
    OrderMenu(OrderController& controller, ConsoleIO& io);

    void run();

private:
    void handlePlaceOrder();
    void handleListReserved();
    void handleApprove();
    void handleReject();

    OrderController& controller_;
    ConsoleIO& io_;
};
