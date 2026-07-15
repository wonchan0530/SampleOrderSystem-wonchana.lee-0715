#pragma once

#include <algorithm>
#include <vector>

// Shared "existing max id + 1" policy used by every Repository's auto-
// increment id. Extracted once three Repositories (Sample/Order/
// ProductionQueue would have, if jobs had ids) needed the identical
// four-line loop -- see design/phase9-design.md SS2.
template <typename T, typename IdExtractor>
int computeNextId(const std::vector<T>& items, IdExtractor idOf) {
    int maxId = 0;
    for (const auto& item : items) {
        maxId = std::max(maxId, idOf(item));
    }
    return maxId + 1;
}
