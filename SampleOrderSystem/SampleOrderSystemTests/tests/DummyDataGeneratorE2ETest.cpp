#include "testing.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#include "Repository/OrderRepository.h"
#include "Repository/SampleRepository.h"
#include "Storage/PathUtil.h"

namespace {

// Runs the built generator via std::system(). Arguments are passed
// unquoted -- none of our paths contain spaces, and quoting them trips a
// well-known Windows CRT system() footgun: when the command string starts
// with a quote, cmd.exe's nested-quote parsing can fail to locate the
// program at all (verified by reproducing it standalone during Phase 7's
// code review). Quoting isn't needed here, so it's simplest to just omit it.
std::string generatorCommand(const std::string& args) {
    const auto exePath = PathUtil::executableDir() / "SampleOrderSystemDummyDataGenerator.exe";
    return exePath.string() + " " + args;
}

}  // namespace

TEST(dummyDataGeneratorE2E_generatesReferentiallyValidDataAndExitsZero) {
    const auto dataDir = std::filesystem::path("test_data") / "dummy_e2e_valid";
    std::filesystem::remove_all(dataDir);

    const int exitCode = std::system(generatorCommand("5 10 42 " + dataDir.string()).c_str());
    EXPECT_EQ(exitCode, 0);

    SampleRepository sampleRepo(dataDir / "samples.json");
    OrderRepository orderRepo(dataDir / "orders.json", sampleRepo);

    const auto samples = sampleRepo.findAll();
    EXPECT_EQ(samples.size(), static_cast<size_t>(5));

    const auto orders = orderRepo.findAll();
    EXPECT_EQ(orders.size(), static_cast<size_t>(10));
    for (const auto& order : orders) {
        bool referencesRealSample = false;
        for (const auto& sample : samples) {
            if (sample.id == order.sampleId) {
                referencesRealSample = true;
                break;
            }
        }
        EXPECT_TRUE(referencesRealSample);
    }

    std::filesystem::remove_all(dataDir);
}

TEST(dummyDataGeneratorE2E_sameSeedIsReproducible) {
    const auto dataDirA = std::filesystem::path("test_data") / "dummy_e2e_repro_a";
    const auto dataDirB = std::filesystem::path("test_data") / "dummy_e2e_repro_b";
    std::filesystem::remove_all(dataDirA);
    std::filesystem::remove_all(dataDirB);

    std::system(generatorCommand("4 6 777 " + dataDirA.string()).c_str());
    std::system(generatorCommand("4 6 777 " + dataDirB.string()).c_str());

    SampleRepository sampleRepoA(dataDirA / "samples.json");
    SampleRepository sampleRepoB(dataDirB / "samples.json");
    const auto samplesA = sampleRepoA.findAll();
    const auto samplesB = sampleRepoB.findAll();

    EXPECT_EQ(samplesA.size(), samplesB.size());
    for (size_t i = 0; i < samplesA.size(); ++i) {
        EXPECT_EQ(samplesA[i].name, samplesB[i].name);
        EXPECT_EQ(samplesA[i].stock, samplesB[i].stock);
    }

    std::filesystem::remove_all(dataDirA);
    std::filesystem::remove_all(dataDirB);
}

TEST(dummyDataGeneratorE2E_rejectsNonPositiveCountsWithNonZeroExit) {
    const int exitCode = std::system(generatorCommand("0 10").c_str());
    EXPECT_TRUE(exitCode != 0);
}
