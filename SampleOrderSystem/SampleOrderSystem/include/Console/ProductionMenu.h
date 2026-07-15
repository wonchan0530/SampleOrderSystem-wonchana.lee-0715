#pragma once

#include "Console/ConsoleIO.h"
#include "Controller/ProductionController.h"

class ProductionMenu {
public:
    ProductionMenu(ProductionController& controller, ConsoleIO& io);

    void run();

private:
    void handleCurrentStatus();
    void handleWaitingQueue();

    ProductionController& controller_;
    ConsoleIO& io_;
};
