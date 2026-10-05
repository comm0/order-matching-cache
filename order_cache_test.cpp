#include <string>
#include <vector>
#include "order_cache.h"
#include "gtest/gtest.h"

class order_cache_test_t : public ::testing::Test {
protected:
    order_cache_t cache;

    void SetUp() override {
    }
};

TEST_F(order_cache_test_t, example_add_order) {
    cache.add_order(order_t{"Order1", "BondA", "Buy", 7000, "Trader1", "FirmA"});
    ASSERT_EQ(cache.get_all_orders().size(), 1);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
