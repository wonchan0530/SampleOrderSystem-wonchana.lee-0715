#include "testing.h"

#include <filesystem>
#include <fstream>

#include "Model/Sample.h"
#include "Storage/JsonFileStore.h"

namespace {

std::filesystem::path testFile(const std::string& name) {
    return std::filesystem::path("test_data") / name;
}

}  // namespace

TEST(jsonFileStore_missingFileReturnsEmpty) {
    const auto path = testFile("missing_samples.json");
    std::filesystem::remove(path);

    JsonFileStore<Sample> store(path);
    EXPECT_TRUE(store.load().empty());
}

TEST(jsonFileStore_emptyFileReturnsEmpty) {
    const auto path = testFile("empty_samples.json");
    std::filesystem::create_directories(path.parent_path());
    { std::ofstream out(path); }

    JsonFileStore<Sample> store(path);
    EXPECT_TRUE(store.load().empty());

    std::filesystem::remove(path);
}

TEST(jsonFileStore_corruptedFileReturnsEmpty) {
    const auto path = testFile("corrupted_samples.json");
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream out(path);
        out << "{ this is not valid json";
    }

    JsonFileStore<Sample> store(path);
    EXPECT_TRUE(store.load().empty());

    std::filesystem::remove(path);
}

TEST(jsonFileStore_saveThenLoadRoundTrips) {
    const auto path = testFile("roundtrip_samples.json");
    std::filesystem::remove(path);

    JsonFileStore<Sample> store(path);
    const std::vector<Sample> samples = {
        {1, "WaferA", 10.0, 0.9, 5},
        {2, "WaferB", 20.0, 0.8, 0},
    };
    store.save(samples);

    JsonFileStore<Sample> reloaded(path);
    const auto loaded = reloaded.load();
    EXPECT_EQ(loaded.size(), static_cast<size_t>(2));
    EXPECT_EQ(loaded[0].name, "WaferA");
    EXPECT_EQ(loaded[1].stock, 0);

    std::filesystem::remove(path);
}

TEST(jsonFileStore_createsParentDirectoryOnSave) {
    const auto dir = std::filesystem::path("test_data") / "nested" / "dir";
    const auto path = dir / "samples.json";
    std::filesystem::remove_all(std::filesystem::path("test_data") / "nested");

    JsonFileStore<Sample> store(path);
    store.save({});
    EXPECT_TRUE(std::filesystem::exists(path));

    std::filesystem::remove_all(std::filesystem::path("test_data") / "nested");
}
