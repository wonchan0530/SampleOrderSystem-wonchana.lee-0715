#pragma once

#include "Console/ConsoleIO.h"
#include "Repository/SampleRepository.h"

class SampleMenu {
public:
    SampleMenu(SampleRepository& repo, ConsoleIO& io);

    void run();

private:
    void handleRegister();
    void handleList();
    void handleSearch();

    SampleRepository& repo_;
    ConsoleIO& io_;
};
