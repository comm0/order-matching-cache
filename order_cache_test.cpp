#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "order_cache.h"
#include "gtest/gtest.h"

namespace {

struct order_snapshot_t
{
  std::string order_id_;
  std::string security_id_;
  std::string side_;
  unsigned int quantity_;
  std::string user_;
  std::string company_;
};

order_snapshot_t snapshot_order(const order_t& order_p)
{
  const std::string order_id = order_p.order_id();
  const std::string security_id = order_p.security_id();
  const std::string side = order_p.side();
  const unsigned int quantity = order_p.quantity();
  const std::string user = order_p.user();
  const std::string company = order_p.company();
  return {order_id, security_id, side, quantity, user, company};
}

void expect_snapshot_equal(const order_snapshot_t& actual_p, const order_snapshot_t& expected_p)
{
  EXPECT_EQ(actual_p.order_id_, expected_p.order_id_);
  EXPECT_EQ(actual_p.security_id_, expected_p.security_id_);
  EXPECT_EQ(actual_p.side_, expected_p.side_);
  EXPECT_EQ(actual_p.quantity_, expected_p.quantity_);
  EXPECT_EQ(actual_p.user_, expected_p.user_);
  EXPECT_EQ(actual_p.company_, expected_p.company_);
}

void expect_orders_equal(const std::vector<order_t>& actual_p, const std::vector<order_t>& expected_p)
{
  ASSERT_EQ(actual_p.size(), expected_p.size());

  std::unordered_map<std::string, order_snapshot_t> actual_by_id;
  for (const order_t& order : actual_p) {
    const order_snapshot_t snapshot = snapshot_order(order);
    actual_by_id.emplace(snapshot.order_id_, snapshot);
  }

  for (const order_t& order : expected_p) {
    const order_snapshot_t expected = snapshot_order(order);
    const auto actual_iterator = actual_by_id.find(expected.order_id_);
    ASSERT_NE(actual_iterator, actual_by_id.end());
    expect_snapshot_equal(actual_iterator->second, expected);
  }
}

class order_cache_test_t : public ::testing::Test
{
 protected:
  void expect_invalid_add_leaves_cache_unchanged(order_t invalid_order_p)
  {
    const std::vector<order_t> before = cache_.get_all_orders();
    EXPECT_THROW(cache_.add_order(std::move(invalid_order_p)), std::invalid_argument);
    expect_orders_equal(cache_.get_all_orders(), before);
  }

  order_cache_t cache_;
};

struct invalid_order_case_t
{
  std::string name_;
  order_t order_;
};

class invalid_order_test_t
    : public order_cache_test_t,
      public ::testing::WithParamInterface<invalid_order_case_t>
{
};

std::vector<invalid_order_case_t> invalid_orders()
{
  return {
      {"empty_order_id", {"", "Security2", "Sell", 200, "User2", "Company2"}},
      {"empty_security_id", {"Order2", "", "Sell", 200, "User2", "Company2"}},
      {"empty_user", {"Order2", "Security2", "Sell", 200, "", "Company2"}},
      {"empty_company", {"Order2", "Security2", "Sell", 200, "User2", ""}},
      {"invalid_side", {"Order2", "Security2", "BUY", 200, "User2", "Company2"}},
      {"zero_quantity", {"Order2", "Security2", "Sell", 0, "User2", "Company2"}},
      {"lowercase_buy", {"Order2", "Security2", "buy", 200, "User2", "Company2"}},
      {"sell_trailing_space", {"Order2", "Security2", "Sell ", 200, "User2", "Company2"}}};
}

std::string invalid_order_name(
    const ::testing::TestParamInfo<invalid_order_case_t>& parameter_info_p)
{
  return parameter_info_p.param.name_;
}

TEST_F(order_cache_test_t, get_all_orders_returns_empty_cache)
{
  EXPECT_TRUE(cache_.get_all_orders().empty());
}

TEST_F(order_cache_test_t, add_order_returns_one_matching_order)
{
  const order_t order{"Order1", "Security1", "Buy", 100, "User1", "Company1"};
  cache_.add_order(order);

  expect_orders_equal(cache_.get_all_orders(), {order});
}

TEST_F(order_cache_test_t, add_order_returns_several_matching_orders)
{
  const std::vector<order_t> orders{
      {"Order1", "Security1", "Buy", 100, "User1", "Company1"},
      {"Order2", "Security1", "Sell", 200, "User2", "Company2"},
      {"Order3", "Security2", "Buy", 300, "User3", "Company1"}};

  for (const order_t& order : orders) {
    cache_.add_order(order);
  }

  expect_orders_equal(cache_.get_all_orders(), orders);
}

TEST_P(invalid_order_test_t, add_order_rejects_invalid_order_without_change)
{
  cache_.add_order(order_t{"Existing", "Security1", "Buy", 100, "User1", "Company1"});
  expect_invalid_add_leaves_cache_unchanged(GetParam().order_);
}

INSTANTIATE_TEST_SUITE_P(
    invalid_orders,
    invalid_order_test_t,
    ::testing::ValuesIn(invalid_orders()),
    invalid_order_name);

TEST_F(order_cache_test_t, duplicate_id_is_rejected_and_id_can_be_reused_after_cancel)
{
  const order_t original{"Order1", "Security1", "Buy", 100, "User1", "Company1"};
  cache_.add_order(original);

  expect_invalid_add_leaves_cache_unchanged(order_t{"Order1", "Security2", "Sell", 200, "User2", "Company2"});

  cache_.cancel_order("Order1");
  const order_t replacement{"Order1", "Security2", "Sell", 200, "User2", "Company2"};
  EXPECT_NO_THROW(cache_.add_order(replacement));
  expect_orders_equal(cache_.get_all_orders(), {replacement});
}

TEST_F(order_cache_test_t, cancel_order_removes_existing_order_and_is_idempotent)
{
  const order_t kept{"Order1", "Security1", "Buy", 100, "User1", "Company1"};
  const order_t removed{"Order2", "Security1", "Sell", 200, "User2", "Company2"};
  cache_.add_order(kept);
  cache_.add_order(removed);

  cache_.cancel_order("Order2");
  expect_orders_equal(cache_.get_all_orders(), {kept});

  EXPECT_NO_THROW(cache_.cancel_order("Order2"));
  expect_orders_equal(cache_.get_all_orders(), {kept});
}

TEST_F(order_cache_test_t, cancel_order_unknown_id_is_no_op)
{
  const order_t order{"Order1", "Security1", "Buy", 100, "User1", "Company1"};
  cache_.add_order(order);

  EXPECT_NO_THROW(cache_.cancel_order("Missing"));
  expect_orders_equal(cache_.get_all_orders(), {order});
}

TEST_F(order_cache_test_t, cancel_orders_for_user_removes_only_that_users_orders_across_securities)
{
  const order_t removed_first{"Order1", "Security1", "Buy", 100, "User1", "Company1"};
  const order_t kept_first{"Order2", "Security1", "Sell", 200, "User2", "Company2"};
  const order_t removed_second{"Order3", "Security2", "Sell", 300, "User1", "Company1"};
  const order_t kept_second{"Order4", "Security2", "Buy", 400, "User3", "Company3"};

  cache_.add_order(removed_first);
  cache_.add_order(kept_first);
  cache_.add_order(removed_second);
  cache_.add_order(kept_second);

  cache_.cancel_orders_for_user("User1");
  expect_orders_equal(cache_.get_all_orders(), {kept_first, kept_second});
}

TEST_F(order_cache_test_t, cancel_orders_for_unknown_user_is_no_op)
{
  const order_t order{"Order1", "Security1", "Buy", 100, "User1", "Company1"};
  cache_.add_order(order);

  EXPECT_NO_THROW(cache_.cancel_orders_for_user("MissingUser"));
  expect_orders_equal(cache_.get_all_orders(), {order});
}

TEST_F(order_cache_test_t, cancel_orders_for_security_obeys_minimum_quantity_boundary)
{
  const order_t below_boundary{"Order1", "Security1", "Buy", 99, "User1", "Company1"};
  const order_t at_boundary{"Order2", "Security1", "Sell", 100, "User2", "Company2"};
  const order_t above_boundary{"Order3", "Security1", "Buy", 101, "User3", "Company3"};
  const order_t other_security{"Order4", "Security2", "Sell", 500, "User4", "Company4"};

  cache_.add_order(below_boundary);
  cache_.add_order(at_boundary);
  cache_.add_order(above_boundary);
  cache_.add_order(other_security);

  cache_.cancel_orders_for_security_with_minimum_quantity("Security1", 100);
  expect_orders_equal(cache_.get_all_orders(), {below_boundary, other_security});
}

TEST_F(order_cache_test_t, cancel_orders_for_security_with_zero_minimum_removes_all_for_security)
{
  const order_t removed_first{"Order1", "Security1", "Buy", 1, "User1", "Company1"};
  const order_t removed_second{"Order2", "Security1", "Sell", 200, "User2", "Company2"};
  const order_t kept{"Order3", "Security2", "Buy", 300, "User3", "Company3"};

  cache_.add_order(removed_first);
  cache_.add_order(removed_second);
  cache_.add_order(kept);

  cache_.cancel_orders_for_security_with_minimum_quantity("Security1", 0);
  expect_orders_equal(cache_.get_all_orders(), {kept});
}

TEST_F(order_cache_test_t, cancel_orders_for_unknown_security_is_no_op)
{
  const order_t order{"Order1", "Security1", "Buy", 100, "User1", "Company1"};
  cache_.add_order(order);

  EXPECT_NO_THROW(cache_.cancel_orders_for_security_with_minimum_quantity("MissingSecurity", 0));
  expect_orders_equal(cache_.get_all_orders(), {order});
}

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
