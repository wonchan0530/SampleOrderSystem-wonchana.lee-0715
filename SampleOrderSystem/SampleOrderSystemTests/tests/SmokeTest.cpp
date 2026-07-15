#include "testing.h"

#include <nlohmann/json.hpp>

TEST(smoke_arithmeticSanityCheck) {
    EXPECT_TRUE(1 + 1 == 2);
}

TEST(smoke_vendoredJsonLibraryRoundTrips) {
    nlohmann::json value;
    value["id"] = 1;
    value["name"] = "sample";

    const std::string dumped = value.dump();
    const nlohmann::json reparsed = nlohmann::json::parse(dumped);

    EXPECT_EQ(reparsed["id"].get<int>(), 1);
    EXPECT_EQ(reparsed["name"].get<std::string>(), "sample");
}
