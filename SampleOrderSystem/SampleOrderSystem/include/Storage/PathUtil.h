#pragma once

#include <filesystem>

// Resolves data file locations relative to the running executable (not the
// current working directory), so the app sees the same data regardless of
// where it's launched from.
namespace PathUtil {

std::filesystem::path executableDir();
std::filesystem::path dataDir();

}  // namespace PathUtil
