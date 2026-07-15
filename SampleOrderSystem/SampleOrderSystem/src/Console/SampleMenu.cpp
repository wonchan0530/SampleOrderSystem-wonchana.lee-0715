#include "Console/SampleMenu.h"

#include <sstream>
#include <vector>

namespace {

void printSampleList(ConsoleIO& io, const std::vector<Sample>& samples) {
    if (samples.empty()) {
        io.println("등록된 시료가 없습니다.");
        return;
    }
    for (const auto& s : samples) {
        std::ostringstream line;
        line << "ID:" << s.id << " 이름:" << s.name << " 평균생산시간:" << s.avgProductionTimeMin
             << " 수율:" << s.yieldRate << " 재고:" << s.stock;
        io.println(line.str());
    }
}

}  // namespace

SampleMenu::SampleMenu(SampleRepository& repo, ConsoleIO& io) : repo_(repo), io_(io) {}

void SampleMenu::handleRegister() {
    std::string name = io_.readLine("이름 > ");
    while (name.empty()) {
        io_.println("이름은 비워둘 수 없습니다.");
        name = io_.readLine("이름 > ");
    }
    const double avgProductionTimeMin = io_.readDouble("평균 생산시간 > ");
    const double yieldRate = io_.readDouble("수율(0~1) > ");
    io_.printResult(repo_.create(name, avgProductionTimeMin, yieldRate));
}

void SampleMenu::handleList() {
    printSampleList(io_, repo_.findAll());
}

void SampleMenu::handleSearch() {
    const std::string keyword = io_.readLine("검색어(이름 또는 ID) > ");
    const auto results = repo_.search(keyword);
    if (results.empty()) {
        io_.println("일치하는 시료가 없습니다.");
        return;
    }
    printSampleList(io_, results);
}

void SampleMenu::run() {
    try {
        while (true) {
            io_.println("--- 시료관리 ---");
            io_.println("1. 시료 등록");
            io_.println("2. 시료 조회");
            io_.println("3. 시료 검색");
            io_.println("0. 뒤로");
            const int choice = io_.readInt("선택 > ");
            if (choice == 1) {
                handleRegister();
            } else if (choice == 2) {
                handleList();
            } else if (choice == 3) {
                handleSearch();
            } else if (choice == 0) {
                return;
            } else {
                io_.println("잘못된 선택입니다.");
            }
        }
    } catch (const EofException&) {
        return;
    }
}
