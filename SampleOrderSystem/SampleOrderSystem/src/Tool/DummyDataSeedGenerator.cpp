#include "Tool/DummyDataSeedGenerator.h"

#include <stdexcept>

namespace DummyDataSeedGenerator {

namespace {

const std::vector<std::string> kCustomerNames = {"Acme", "Globex", "Initech", "Umbrella", "Stark"};

}  // namespace

std::vector<SampleSeed> generateSamples(std::mt19937& rng, int count) {
    std::uniform_real_distribution<double> timeDist(1.0, 30.0);
    std::uniform_real_distribution<double> yieldDist(0.5, 1.0);
    std::uniform_int_distribution<int> stockDist(0, 50);

    std::vector<SampleSeed> result;
    result.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        result.push_back({
            "Sample-" + std::to_string(i + 1),
            timeDist(rng),
            yieldDist(rng),
            stockDist(rng),
        });
    }
    return result;
}

std::vector<OrderSeed> generateOrders(std::mt19937& rng, const std::vector<int>& sampleIds, int count) {
    if (sampleIds.empty()) {
        throw std::invalid_argument("sampleIds must not be empty");
    }

    std::uniform_int_distribution<size_t> sampleIndexDist(0, sampleIds.size() - 1);
    std::uniform_int_distribution<int> quantityDist(1, 20);
    std::uniform_int_distribution<size_t> customerDist(0, kCustomerNames.size() - 1);

    std::vector<OrderSeed> result;
    result.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        result.push_back({
            sampleIds[sampleIndexDist(rng)],
            kCustomerNames[customerDist(rng)],
            quantityDist(rng),
        });
    }
    return result;
}

}  // namespace DummyDataSeedGenerator
