#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Model/Order.h"
#include "Repository/RepositoryResult.h"
#include "Repository/SampleRepository.h"
#include "Storage/JsonFileStore.h"

// status is intentionally not patchable here -- state transitions
// (approve/reject/production completion) are dedicated methods added in
// later phases, not a generic patch.
struct OrderUpdate {
    std::optional<std::string> customerName;
    std::optional<int> quantity;
};

class OrderRepository {
public:
    // sampleRepository is referenced (not owned) purely to check referential
    // integrity on create -- a one-way dependency, mirroring the
    // Data_Persistence POC.
    OrderRepository(std::filesystem::path dataFile, const SampleRepository& sampleRepository);

    std::vector<Order> findAll() const;
    std::optional<Order> findById(int orderId) const;
    std::vector<Order> findBySampleId(int sampleId) const;

    RepositoryResult create(int sampleId, const std::string& customerName, int quantity);
    RepositoryResult update(int orderId, const OrderUpdate& patch);
    RepositoryResult remove(int orderId);

    // Legality of each transition is enforced here; the decision of *when*
    // to call them (stock policy, wall-clock completion) belongs to
    // Controller layers (OrderController / ProductionController).
    RepositoryResult markConfirmed(int orderId);  // allowed from RESERVED or PRODUCING
    RepositoryResult markProducing(int orderId);  // allowed from RESERVED
    RepositoryResult markRejected(int orderId);   // allowed from RESERVED

private:
    JsonFileStore<Order> store_;
    std::vector<Order> cache_;
    int nextId_ = 1;
    const SampleRepository& sampleRepository_;

    void recalcNextId();
    void persist() const;
};
