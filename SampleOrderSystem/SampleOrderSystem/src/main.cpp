#include "Console/ConsoleIO.h"
#include "Console/SampleMenu.h"
#include "Repository/SampleRepository.h"
#include "Storage/PathUtil.h"

int main() {
    ConsoleIO io;
    SampleRepository sampleRepo(PathUtil::dataDir() / "samples.json");
    SampleMenu sampleMenu(sampleRepo, io);

    while (true) {
        io.println("=== 반도체 시료 생산주문관리 시스템 ===");
        io.println("1. 시료관리");
        io.println("0. 종료");
        try {
            const int choice = io.readInt("선택 > ");
            if (choice == 1) {
                sampleMenu.run();
            } else if (choice == 0) {
                break;
            } else {
                io.println("잘못된 선택입니다.");
            }
        } catch (const EofException&) {
            break;
        }
    }
    return 0;
}
