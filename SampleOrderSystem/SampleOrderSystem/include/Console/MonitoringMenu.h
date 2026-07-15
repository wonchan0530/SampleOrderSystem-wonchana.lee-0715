#pragma once

#include "Console/ConsoleIO.h"
#include "Controller/MonitoringController.h"

class MonitoringMenu {
public:
    MonitoringMenu(MonitoringController& controller, ConsoleIO& io);

    void run();

private:
    void handleOrderCounts();
    void handleStockLevels();

    MonitoringController& controller_;
    ConsoleIO& io_;
};
