#pragma once

#include "Console/ConsoleIO.h"
#include "Controller/OrderController.h"

class ShippingMenu {
public:
    ShippingMenu(OrderController& controller, ConsoleIO& io);

    void run();

private:
    void handleListConfirmed();
    void handleRelease();

    OrderController& controller_;
    ConsoleIO& io_;
};
