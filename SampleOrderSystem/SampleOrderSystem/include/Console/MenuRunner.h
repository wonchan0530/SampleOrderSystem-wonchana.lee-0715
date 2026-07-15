#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "Console/ConsoleIO.h"

// One numbered item in a menu: printed as "<choice>. <label>" and invoked
// via handler when selected. "0. 뒤로" and the EofException-catches-once
// loop are handled by runMenu itself so every *Menu::run() doesn't repeat
// the identical skeleton (extracted once five menus needed it -- see
// design/phase9-design.md SS2).
struct MenuItem {
    int choice;
    std::string label;
    std::function<void()> handler;
};

inline void runMenu(ConsoleIO& io, const std::string& title, const std::vector<MenuItem>& items) {
    try {
        while (true) {
            io.println("--- " + title + " ---");
            for (const auto& item : items) {
                io.println(std::to_string(item.choice) + ". " + item.label);
            }
            io.println("0. 뒤로");

            const int choice = io.readInt("선택 > ");
            if (choice == 0) {
                return;
            }
            const auto it = std::find_if(items.begin(), items.end(),
                                          [choice](const MenuItem& item) { return item.choice == choice; });
            if (it != items.end()) {
                it->handler();
            } else {
                io.println("잘못된 선택입니다.");
            }
        }
    } catch (const EofException&) {
        return;
    }
}
