#include "testing.h"

#include <filesystem>

#include "Repository/SampleRepository.h"

namespace {

std::filesystem::path testFile(const std::string& name) {
    return std::filesystem::path("test_data") / name;
}

}  // namespace

TEST(sampleRepository_createAndFindById) {
    const auto path = testFile("sample_repo_create.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);

    const auto created = repo.create("WaferA", 10.0, 0.9);
    EXPECT_TRUE(created.success);

    const auto all = repo.findAll();
    EXPECT_EQ(all.size(), static_cast<size_t>(1));
    EXPECT_EQ(all[0].stock, 0);

    const auto found = repo.findById(all[0].id);
    EXPECT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "WaferA");

    std::filesystem::remove(path);
}

TEST(sampleRepository_rejectsInvalidYieldAndProductionTime) {
    const auto path = testFile("sample_repo_validate.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);

    EXPECT_TRUE(!repo.create("Bad", 0.0, 0.5).success);
    EXPECT_TRUE(!repo.create("Bad", -1.0, 0.5).success);
    EXPECT_TRUE(!repo.create("Bad", 10.0, 0.0).success);
    EXPECT_TRUE(!repo.create("Bad", 10.0, 1.5).success);
    EXPECT_TRUE(repo.findAll().empty());

    std::filesystem::remove(path);
}

TEST(sampleRepository_searchMatchesIdExactlyAndNamePartially) {
    const auto path = testFile("sample_repo_search.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);
    repo.create("WaferAlpha", 10.0, 0.9);
    repo.create("WaferBeta", 12.0, 0.85);

    const auto byName = repo.search("alpha");
    EXPECT_EQ(byName.size(), static_cast<size_t>(1));
    EXPECT_EQ(byName[0].name, "WaferAlpha");

    const int betaId = repo.findAll()[1].id;
    const auto byId = repo.search(std::to_string(betaId));
    EXPECT_EQ(byId.size(), static_cast<size_t>(1));
    EXPECT_EQ(byId[0].name, "WaferBeta");

    std::filesystem::remove(path);
}

TEST(sampleRepository_updateAppliesPartialPatchOnly) {
    const auto path = testFile("sample_repo_update.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);
    repo.create("WaferA", 10.0, 0.9);
    const int id = repo.findAll()[0].id;

    SampleUpdate patch;
    patch.name = "Renamed";
    const auto result = repo.update(id, patch);
    EXPECT_TRUE(result.success);

    const auto found = repo.findById(id);
    EXPECT_EQ(found->name, "Renamed");
    EXPECT_EQ(found->avgProductionTimeMin, 10.0);
    EXPECT_EQ(found->yieldRate, 0.9);

    std::filesystem::remove(path);
}

TEST(sampleRepository_updateRejectsUnknownId) {
    const auto path = testFile("sample_repo_update_unknown.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);

    SampleUpdate patch;
    patch.name = "Whatever";
    const auto result = repo.update(9999, patch);
    EXPECT_TRUE(!result.success);

    std::filesystem::remove(path);
}

TEST(sampleRepository_removeDeletesSample) {
    const auto path = testFile("sample_repo_remove.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);
    repo.create("WaferA", 10.0, 0.9);
    const int id = repo.findAll()[0].id;

    const auto result = repo.remove(id);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(repo.findAll().empty());
    EXPECT_TRUE(!repo.remove(id).success);

    std::filesystem::remove(path);
}

TEST(sampleRepository_adjustStockIncreasesAndDecreases) {
    const auto path = testFile("sample_repo_stock.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);
    repo.create("WaferA", 10.0, 0.9);
    const int id = repo.findAll()[0].id;

    EXPECT_TRUE(repo.adjustStock(id, 10).success);
    EXPECT_EQ(repo.findById(id)->stock, 10);

    EXPECT_TRUE(repo.adjustStock(id, -4).success);
    EXPECT_EQ(repo.findById(id)->stock, 6);

    std::filesystem::remove(path);
}

TEST(sampleRepository_adjustStockRejectsNegativeResult) {
    const auto path = testFile("sample_repo_stock_negative.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);
    repo.create("WaferA", 10.0, 0.9);
    const int id = repo.findAll()[0].id;

    const auto result = repo.adjustStock(id, -1);
    EXPECT_TRUE(!result.success);
    EXPECT_EQ(repo.findById(id)->stock, 0);

    std::filesystem::remove(path);
}

TEST(sampleRepository_dataPersistsAcrossInstances) {
    const auto path = testFile("sample_repo_persistence.json");
    std::filesystem::remove(path);

    {
        SampleRepository repo(path);
        repo.create("WaferA", 10.0, 0.9);
        repo.adjustStock(repo.findAll()[0].id, 3);
    }

    {
        SampleRepository reopened(path);
        const auto all = reopened.findAll();
        EXPECT_EQ(all.size(), static_cast<size_t>(1));
        EXPECT_EQ(all[0].name, "WaferA");
        EXPECT_EQ(all[0].stock, 3);
    }

    std::filesystem::remove(path);
}
