#pragma once

#include <vector>

#include "Model/Order.h"
#include "Model/Sample.h"
#include "Repository/OrderRepository.h"
#include "Repository/SampleRepository.h"

struct OrderStatusCounts {
    int reserved = 0;
    int producing = 0;
    int confirmed = 0;
    int release = 0;
    // REJECTED is intentionally excluded (docs/04-모니터링.md).
};

enum class StockLevel { Depleted, Insufficient, Sufficient };  // 고갈 / 부족 / 여유

struct SampleStockStatus {
    Sample sample;
    int pendingQuantity;  // sum of RESERVED+PRODUCING+CONFIRMED order quantities for this sample
    StockLevel level;
};

// Read-only aggregation across Sample and Order -- the first place two
// repositories are genuinely composed for reporting rather than lifecycle
// management (see design/phase6-design.md SS2).
class MonitoringController {
public:
    MonitoringController(const SampleRepository& sampleRepo, const OrderRepository& orderRepo);

    OrderStatusCounts countOrdersByStatus() const;
    std::vector<SampleStockStatus> stockStatusBySample() const;

private:
    const SampleRepository& sampleRepo_;
    const OrderRepository& orderRepo_;
};
