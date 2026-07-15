#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Model/Sample.h"
#include "Repository/RepositoryResult.h"
#include "Storage/JsonFileStore.h"

// Fields left as nullopt are left unchanged by SampleRepository::update.
// stock is intentionally not patchable here -- it only changes via
// adjustStock, which is driven by order approval/production-completion
// logic in higher layers (Phase 3-4), not by direct user edits.
struct SampleUpdate {
    std::optional<std::string> name;
    std::optional<double> avgProductionTimeMin;
    std::optional<double> yieldRate;
};

class SampleRepository {
public:
    explicit SampleRepository(std::filesystem::path dataFile);

    std::vector<Sample> findAll() const;
    std::optional<Sample> findById(int id) const;
    std::vector<Sample> search(const std::string& keyword) const;

    RepositoryResult create(const std::string& name, double avgProductionTimeMin, double yieldRate);
    RepositoryResult update(int id, const SampleUpdate& patch);
    RepositoryResult remove(int id);

    // Applies a stock delta decided by the caller (this repository does not
    // judge stock policy itself -- see design/phase1-design.md SS4).
    RepositoryResult adjustStock(int id, int delta);

private:
    JsonFileStore<Sample> store_;
    std::vector<Sample> cache_;
    int nextId_ = 1;

    void recalcNextId();
    void persist() const;
    static RepositoryResult validateFields(double avgProductionTimeMin, double yieldRate);
};
