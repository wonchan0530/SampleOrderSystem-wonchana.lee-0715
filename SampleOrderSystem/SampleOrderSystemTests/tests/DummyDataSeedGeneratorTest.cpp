#include "testing.h"

#include <algorithm>
#include <random>
#include <stdexcept>

#include "Tool/DummyDataSeedGenerator.h"

TEST(dummyDataSeedGenerator_generatesRequestedSampleCount) {
    std::mt19937 rng(42);
    const auto samples = DummyDataSeedGenerator::generateSamples(rng, 15);
    EXPECT_EQ(samples.size(), static_cast<size_t>(15));
}

TEST(dummyDataSeedGenerator_sampleValuesWithinExpectedRanges) {
    std::mt19937 rng(7);
    const auto samples = DummyDataSeedGenerator::generateSamples(rng, 50);
    for (const auto& s : samples) {
        EXPECT_TRUE(s.avgProductionTimeMin >= 1.0 && s.avgProductionTimeMin <= 30.0);
        EXPECT_TRUE(s.yieldRate >= 0.5 && s.yieldRate <= 1.0);
        EXPECT_TRUE(s.initialStock >= 0 && s.initialStock <= 50);
    }
}

TEST(dummyDataSeedGenerator_ordersOnlyReferenceGivenSampleIds) {
    std::mt19937 rng(3);
    const std::vector<int> sampleIds = {10, 20, 30};
    const auto orders = DummyDataSeedGenerator::generateOrders(rng, sampleIds, 40);
    EXPECT_EQ(orders.size(), static_cast<size_t>(40));
    for (const auto& o : orders) {
        const bool found = std::find(sampleIds.begin(), sampleIds.end(), o.sampleId) != sampleIds.end();
        EXPECT_TRUE(found);
        EXPECT_TRUE(o.quantity >= 1 && o.quantity <= 20);
    }
}

TEST(dummyDataSeedGenerator_sameSeedProducesSameSamples) {
    std::mt19937 rngA(123);
    std::mt19937 rngB(123);
    const auto a = DummyDataSeedGenerator::generateSamples(rngA, 10);
    const auto b = DummyDataSeedGenerator::generateSamples(rngB, 10);

    EXPECT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].name, b[i].name);
        EXPECT_TRUE(a[i].avgProductionTimeMin == b[i].avgProductionTimeMin);
        EXPECT_TRUE(a[i].yieldRate == b[i].yieldRate);
        EXPECT_EQ(a[i].initialStock, b[i].initialStock);
    }
}

TEST(dummyDataSeedGenerator_sameSeedProducesSameOrders) {
    const std::vector<int> sampleIds = {1, 2, 3, 4};
    std::mt19937 rngA(99);
    std::mt19937 rngB(99);
    const auto a = DummyDataSeedGenerator::generateOrders(rngA, sampleIds, 20);
    const auto b = DummyDataSeedGenerator::generateOrders(rngB, sampleIds, 20);

    EXPECT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].sampleId, b[i].sampleId);
        EXPECT_EQ(a[i].customerName, b[i].customerName);
        EXPECT_EQ(a[i].quantity, b[i].quantity);
    }
}

TEST(dummyDataSeedGenerator_throwsWhenNoSampleIdsProvided) {
    std::mt19937 rng(1);
    bool threw = false;
    try {
        DummyDataSeedGenerator::generateOrders(rng, {}, 5);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    EXPECT_TRUE(threw);
}
