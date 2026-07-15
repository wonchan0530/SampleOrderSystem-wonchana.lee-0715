#pragma once

#include <random>
#include <string>
#include <vector>

// Pure value-generation logic for the dummy data tool -- no file/repository
// I/O here, so it's unit-testable without touching disk. The tool's main.cpp
// is responsible for actually inserting these through the real Repository/
// Controller APIs (see design/phase7-design.md SS4) so referential integrity
// and validation are enforced by the same code the real app uses.
struct SampleSeed {
    std::string name;
    double avgProductionTimeMin;
    double yieldRate;
    int initialStock;
};

struct OrderSeed {
    int sampleId;
    std::string customerName;
    int quantity;
};

namespace DummyDataSeedGenerator {

std::vector<SampleSeed> generateSamples(std::mt19937& rng, int count);

// Throws std::invalid_argument if sampleIds is empty (an order must
// reference an existing sample).
std::vector<OrderSeed> generateOrders(std::mt19937& rng, const std::vector<int>& sampleIds, int count);

}  // namespace DummyDataSeedGenerator
