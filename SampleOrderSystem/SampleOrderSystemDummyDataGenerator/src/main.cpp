#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "Controller/OrderController.h"
#include "Repository/OrderRepository.h"
#include "Repository/ProductionQueueRepository.h"
#include "Repository/SampleRepository.h"
#include "Storage/PathUtil.h"
#include "Tool/DummyDataSeedGenerator.h"

namespace {

void printUsage() {
    std::cerr << "사용법: SampleOrderSystemDummyDataGenerator [샘플개수=10] [주문개수=30] [seed] [dataDir]\n";
}

}  // namespace

int main(int argc, char** argv) {
    int sampleCount = 10;
    int orderCount = 30;
    unsigned int seed = std::random_device{}();
    bool seedProvided = false;
    std::filesystem::path dataDir = PathUtil::dataDir();

    try {
        if (argc >= 2) {
            sampleCount = std::stoi(argv[1]);
        }
        if (argc >= 3) {
            orderCount = std::stoi(argv[2]);
        }
        if (argc >= 4) {
            seed = static_cast<unsigned int>(std::stoul(argv[3]));
            seedProvided = true;
        }
        if (argc >= 5) {
            dataDir = argv[4];
        }
    } catch (const std::exception&) {
        printUsage();
        return 1;
    }

    if (sampleCount <= 0 || orderCount <= 0) {
        std::cerr << "샘플개수/주문개수는 1 이상이어야 합니다.\n";
        printUsage();
        return 1;
    }

    std::mt19937 rng(seed);

    SampleRepository sampleRepo(dataDir / "samples.json");
    OrderRepository orderRepo(dataDir / "orders.json", sampleRepo);
    ProductionQueueRepository productionQueue(dataDir / "production_queue.json");
    OrderController orderController(orderRepo, sampleRepo, productionQueue);

    // 1) Samples first, so orders below can reference real IDs.
    const auto sampleSeeds = DummyDataSeedGenerator::generateSamples(rng, sampleCount);
    std::vector<int> sampleIds;
    sampleIds.reserve(sampleSeeds.size());
    for (const auto& seedData : sampleSeeds) {
        sampleRepo.create(seedData.name, seedData.avgProductionTimeMin, seedData.yieldRate);
        const int newId = sampleRepo.findAll().back().id;
        sampleRepo.adjustStock(newId, seedData.initialStock);
        sampleIds.push_back(newId);
    }

    // 2) Orders, referencing only the sample IDs just created.
    const auto orderSeeds = DummyDataSeedGenerator::generateOrders(rng, sampleIds, orderCount);
    std::vector<int> orderIds;
    orderIds.reserve(orderSeeds.size());
    for (const auto& seedData : orderSeeds) {
        orderRepo.create(seedData.sampleId, seedData.customerName, seedData.quantity);
        orderIds.push_back(orderRepo.findAll().back().orderId);
    }

    // 3) Approve roughly half the orders through the real business logic,
    //    so the resulting data has a realistic mix of RESERVED/PRODUCING/
    //    CONFIRMED instead of everything sitting at RESERVED.
    std::uniform_real_distribution<double> approveChance(0.0, 1.0);
    int approvedCount = 0;
    for (int orderId : orderIds) {
        if (approveChance(rng) < 0.5 && orderController.approve(orderId).success) {
            ++approvedCount;
        }
    }

    std::cout << "시료 " << sampleIds.size() << "개, 주문 " << orderIds.size() << "개 생성 완료 (그 중 "
              << approvedCount << "건 승인 처리)\n";
    std::cout << "seed=" << seed << (seedProvided ? "" : " (랜덤)") << ", 저장 위치=" << dataDir.string() << "\n";
    return 0;
}
