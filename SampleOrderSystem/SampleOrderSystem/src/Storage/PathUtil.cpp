#include "Storage/PathUtil.h"

#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace PathUtil {

std::filesystem::path executableDir() {
#ifdef _WIN32
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    const std::filesystem::path exePath(buffer.data(), buffer.data() + length);
    return exePath.parent_path();
#else
    return std::filesystem::current_path();
#endif
}

std::filesystem::path dataDir() {
    return executableDir() / "data";
}

}  // namespace PathUtil
