#include "testing.h"

#include <filesystem>

#include "Storage/PathUtil.h"

TEST(pathUtil_executableDirExists) {
    const auto dir = PathUtil::executableDir();
    EXPECT_TRUE(std::filesystem::exists(dir));
    EXPECT_TRUE(std::filesystem::is_directory(dir));
}

TEST(pathUtil_dataDirIsExecutableDirSlashData) {
    const auto dataDir = PathUtil::dataDir();
    const auto expected = PathUtil::executableDir() / "data";
    EXPECT_TRUE(dataDir == expected);
}
