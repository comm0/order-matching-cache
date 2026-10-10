#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
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

class storage_test_t : public order_cache_test_t
{
};

class validation_test_t : public order_cache_test_t
{
};

class cancellation_test_t : public order_cache_test_t
{
};

class matching_test_t : public order_cache_test_t
{
};

class performance_test_t : public order_cache_test_t
{
};

class concurrency_test_t : public order_cache_test_t
{
};

template <typename callable_t>
std::chrono::milliseconds time_phase(const std::string& phase_name_p, callable_t&& callable_p)
{
  const auto start = std::chrono::steady_clock::now();
  std::forward<callable_t>(callable_p)();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  std::cout << phase_name_p << ": " << elapsed.count() << " ms\n";
  return elapsed;
}

template <typename callable_t>
void run_in_parallel(std::size_t thread_count_p, callable_t&& callable_p)
{
  std::vector<std::thread> threads;
  threads.reserve(thread_count_p);
  for (std::size_t thread_index = 0; thread_index < thread_count_p; ++thread_index) {
    threads.emplace_back(callable_p, thread_index);
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
}

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

TEST_F(storage_test_t, get_all_orders_returns_empty_cache)
{
  EXPECT_TRUE(cache_.get_all_orders().empty());
}

TEST_F(storage_test_t, add_order_returns_one_matching_order)
{
  const order_t order{"Order1", "Security1", "Buy", 100, "User1", "Company1"};
  cache_.add_order(order);

  expect_orders_equal(cache_.get_all_orders(), {order});
}

TEST_F(storage_test_t, add_order_returns_several_matching_orders)
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
    validation,
    invalid_order_test_t,
    ::testing::ValuesIn(invalid_orders()),
    invalid_order_name);

TEST_F(validation_test_t, duplicate_id_is_rejected_and_id_can_be_reused_after_cancel)
{
  const order_t original{"Order1", "Security1", "Buy", 100, "User1", "Company1"};
  cache_.add_order(original);

  expect_invalid_add_leaves_cache_unchanged(order_t{"Order1", "Security2", "Sell", 200, "User2", "Company2"});

  cache_.cancel_order("Order1");
  const order_t replacement{"Order1", "Security2", "Sell", 200, "User2", "Company2"};
  EXPECT_NO_THROW(cache_.add_order(replacement));
  expect_orders_equal(cache_.get_all_orders(), {replacement});
}

TEST_F(cancellation_test_t, cancel_order_removes_existing_order_and_is_idempotent)
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

TEST_F(cancellation_test_t, cancel_order_unknown_id_is_no_op)
{
  const order_t order{"Order1", "Security1", "Buy", 100, "User1", "Company1"};
  cache_.add_order(order);

  EXPECT_NO_THROW(cache_.cancel_order("Missing"));
  expect_orders_equal(cache_.get_all_orders(), {order});
}

TEST_F(cancellation_test_t, cancel_orders_for_user_removes_only_that_users_orders_across_securities)
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

TEST_F(cancellation_test_t, cancel_orders_for_unknown_user_is_no_op)
{
  const order_t order{"Order1", "Security1", "Buy", 100, "User1", "Company1"};
  cache_.add_order(order);

  EXPECT_NO_THROW(cache_.cancel_orders_for_user("MissingUser"));
  expect_orders_equal(cache_.get_all_orders(), {order});
}

TEST_F(cancellation_test_t, cancel_orders_for_security_obeys_minimum_quantity_boundary)
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

TEST_F(cancellation_test_t, cancel_orders_for_security_with_zero_minimum_removes_all_for_security)
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

TEST_F(cancellation_test_t, cancel_orders_for_unknown_security_is_no_op)
{
  const order_t order{"Order1", "Security1", "Buy", 100, "User1", "Company1"};
  cache_.add_order(order);

  EXPECT_NO_THROW(cache_.cancel_orders_for_security_with_minimum_quantity("MissingSecurity", 0));
  expect_orders_equal(cache_.get_all_orders(), {order});
}

struct matching_scenario_t
{
  std::string name_;
  std::vector<order_t> orders_;
  std::vector<std::pair<std::string, unsigned int>> expected_matching_;
};

class matching_scenario_test_t
    : public order_cache_test_t,
      public ::testing::WithParamInterface<matching_scenario_t>
{
};

std::vector<matching_scenario_t> matching_scenarios()
{
  return {
      {"scenario_1",
       {{"Order1", "BondA", "Buy", 7000, "Trader1", "FirmA"},
        {"Order2", "BondB", "Sell", 21000, "Trader2", "FirmB"},
        {"Order3", "BondA", "Sell", 3500, "Trader3", "FirmA"},
        {"Order4", "BondB", "Buy", 4200, "Trader4", "FirmC"},
        {"Order5", "BondB", "Buy", 700, "Trader5", "FirmB"},
        {"Order6", "BondC", "Buy", 7000, "Trader6", "FirmD"},
        {"Order7", "BondB", "Buy", 14000, "Trader7", "FirmE"},
        {"Order8", "BondB", "Sell", 35000, "Trader8", "FirmE"}},
       {{"BondA", 0}, {"BondB", 18900}, {"BondC", 0}}},
      {"scenario_2",
       {{"Order1", "BondA", "Sell", 700, "Trader10", "Firm2"},
        {"Order2", "BondC", "Sell", 1400, "Trader8", "Firm2"},
        {"Order3", "BondA", "Buy", 2100, "Trader13", "Firm2"},
        {"Order4", "BondB", "Sell", 2800, "Trader12", "Firm2"},
        {"Order5", "BondC", "Sell", 3500, "Trader7", "Firm2"},
        {"Order6", "BondC", "Buy", 4200, "Trader3", "Firm1"},
        {"Order7", "BondA", "Sell", 4900, "Trader10", "Firm2"},
        {"Order8", "BondA", "Sell", 5600, "Trader2", "Firm1"},
        {"Order9", "BondB", "Buy", 6300, "Trader6", "Firm2"},
        {"Order10", "BondB", "Sell", 7000, "Trader5", "Firm1"},
        {"Order11", "BondA", "Sell", 7700, "Trader13", "Firm2"},
        {"Order12", "BondB", "Buy", 8400, "Trader9", "Firm2"},
        {"Order13", "BondA", "Sell", 9100, "Trader1", "Firm1"}},
       {{"BondA", 2100}, {"BondB", 7000}, {"BondC", 4200}}},
      {"scenario_3",
       {{"Order1", "BondC", "Sell", 700, "Trader1", "Firm1"},
        {"Order2", "BondC", "Sell", 1400, "Trader3", "Firm2"},
        {"Order3", "BondA", "Buy", 2100, "Trader2", "Firm1"},
        {"Order4", "BondC", "Sell", 2800, "Trader5", "Firm2"},
        {"Order5", "BondB", "Sell", 3500, "Trader2", "Firm1"},
        {"Order6", "BondB", "Buy", 4200, "Trader3", "Firm2"},
        {"Order7", "BondB", "Sell", 4900, "Trader1", "Firm1"},
        {"Order8", "BondA", "Sell", 5600, "Trader2", "Firm1"},
        {"Order9", "BondA", "Buy", 6300, "Trader5", "Firm2"},
        {"Order10", "BondA", "Sell", 7000, "Trader1", "Firm1"},
        {"Order11", "BondB", "Sell", 7700, "Trader6", "Firm2"}},
       {{"BondA", 6300}, {"BondB", 4200}, {"BondC", 0}}},
      {"order_dependent_greedy_trap",
       {{"Order1", "Security1", "Buy", 100, "User1", "CompanyA"},
        {"Order2", "Security1", "Buy", 100, "User2", "CompanyB"},
        {"Order3", "Security1", "Sell", 100, "User3", "CompanyC"},
        {"Order4", "Security1", "Sell", 100, "User4", "CompanyB"}},
       {{"Security1", 200}}},
      {"one_buy_split_across_sells",
       {{"Order1", "Security1", "Buy", 1000, "User1", "CompanyA"},
        {"Order2", "Security1", "Sell", 300, "User2", "CompanyB"},
        {"Order3", "Security1", "Sell", 400, "User3", "CompanyC"}},
       {{"Security1", 700}}},
      {"same_company_never_matches",
       {{"Order1", "Security1", "Buy", 100, "User1", "CompanyA"},
        {"Order2", "Security1", "Sell", 100, "User2", "CompanyA"}},
       {{"Security1", 0}}},
      {"only_one_side",
       {{"Order1", "Security1", "Buy", 100, "User1", "CompanyA"},
        {"Order2", "Security1", "Buy", 100, "User2", "CompanyB"}},
       {{"Security1", 0}}},
      {"unknown_security",
       {{"Order1", "Security1", "Buy", 100, "User1", "CompanyA"}},
       {{"MissingSecurity", 0}}}};
}

std::string matching_scenario_name(
    const ::testing::TestParamInfo<matching_scenario_t>& parameter_info_p)
{
  return parameter_info_p.param.name_;
}

TEST_P(matching_scenario_test_t, matching_size_equals_expected)
{
  for (const order_t& order : GetParam().orders_) {
    cache_.add_order(order);
  }

  for (const auto& [security_id, expected] : GetParam().expected_matching_) {
    EXPECT_EQ(cache_.get_matching_size_for_security(security_id), expected) << security_id;
  }
}

INSTANTIATE_TEST_SUITE_P(
    matching,
    matching_scenario_test_t,
    ::testing::ValuesIn(matching_scenarios()),
    matching_scenario_name);

TEST_F(matching_test_t, matching_follows_cancellations)
{
  cache_.add_order(order_t{"Order1", "Security1", "Buy", 500, "User1", "CompanyA"});
  cache_.add_order(order_t{"Order2", "Security1", "Sell", 300, "User2", "CompanyB"});
  cache_.add_order(order_t{"Order3", "Security1", "Sell", 400, "User3", "CompanyC"});
  EXPECT_EQ(cache_.get_matching_size_for_security("Security1"), 500u);

  cache_.cancel_order("Order3");
  EXPECT_EQ(cache_.get_matching_size_for_security("Security1"), 300u);

  cache_.cancel_orders_for_user("User2");
  EXPECT_EQ(cache_.get_matching_size_for_security("Security1"), 0u);

  cache_.add_order(order_t{"Order4", "Security1", "Sell", 200, "User4", "CompanyD"});
  EXPECT_EQ(cache_.get_matching_size_for_security("Security1"), 200u);

  cache_.cancel_orders_for_security_with_minimum_quantity("Security1", 500);
  EXPECT_EQ(cache_.get_matching_size_for_security("Security1"), 0u);
}

TEST_F(matching_test_t, matching_saturates_instead_of_overflowing)
{
  const unsigned int maximum = std::numeric_limits<unsigned int>::max();
  cache_.add_order(order_t{"Order1", "Security1", "Buy", maximum, "User1", "CompanyA"});
  cache_.add_order(order_t{"Order2", "Security1", "Buy", maximum, "User2", "CompanyB"});
  cache_.add_order(order_t{"Order3", "Security1", "Sell", maximum, "User3", "CompanyC"});
  cache_.add_order(order_t{"Order4", "Security1", "Sell", maximum, "User4", "CompanyD"});

  EXPECT_EQ(cache_.get_matching_size_for_security("Security1"), maximum);
}

struct matching_reference_t
{
  std::uint64_t buy_total_ = 0;
  std::uint64_t sell_total_ = 0;
  std::unordered_map<std::string, std::uint64_t> company_totals_;
};

std::unordered_map<std::string, unsigned int> calculate_reference_matching(
    const std::vector<order_t>& orders_p)
{
  std::unordered_map<std::string, matching_reference_t> references;
  for (const order_t& order : orders_p) {
    const std::string security_id = order.security_id();
    const std::string side = order.side();
    const unsigned int quantity = order.quantity();
    const std::string company = order.company();

    matching_reference_t& reference = references[security_id];
    if (side == "Buy") {
      reference.buy_total_ += quantity;
    } else {
      reference.sell_total_ += quantity;
    }
    reference.company_totals_[company] += quantity;
  }

  std::unordered_map<std::string, unsigned int> matching_by_security;
  matching_by_security.reserve(references.size());
  for (const auto& [security_id, reference] : references) {
    std::uint64_t largest_company_total = 0;
    for (const auto& company_entry : reference.company_totals_) {
      largest_company_total = std::max(largest_company_total, company_entry.second);
    }

    const std::uint64_t matched = std::min({
        reference.buy_total_,
        reference.sell_total_,
        reference.buy_total_ + reference.sell_total_ - largest_company_total});
    matching_by_security.emplace(
        security_id,
        static_cast<unsigned int>(
            std::min<std::uint64_t>(matched, std::numeric_limits<unsigned int>::max())));
  }
  return matching_by_security;
}

void expect_matching_equals_reference(
    order_cache_t& cache_p,
    const std::vector<std::string>& security_ids_p,
    const std::vector<order_t>& orders_p)
{
  const auto expected_matching = calculate_reference_matching(orders_p);
  for (const std::string& security_id : security_ids_p) {
    const auto expected_iterator = expected_matching.find(security_id);
    const unsigned int expected = expected_iterator == expected_matching.end() ? 0 : expected_iterator->second;
    EXPECT_EQ(cache_p.get_matching_size_for_security(security_id), expected) << security_id;
  }
}

TEST_F(concurrency_test_t, concurrent_adds_and_reads_preserve_all_orders)
{
  constexpr std::size_t thread_count = 8;
  constexpr std::size_t orders_per_thread = 2000;
  constexpr std::size_t security_count = 16;

  std::vector<std::string> security_ids;
  security_ids.reserve(security_count);
  for (std::size_t index = 0; index < security_count; ++index) {
    security_ids.push_back("ConcurrentSecurity" + std::to_string(index));
  }

  run_in_parallel(thread_count, [&](std::size_t thread_index_p) {
    for (std::size_t order_index = 0; order_index < orders_per_thread; ++order_index) {
      const std::size_t global_index = thread_index_p * orders_per_thread + order_index;
      cache_.add_order(order_t{
          "ConcurrentOrder" + std::to_string(global_index),
          security_ids[global_index % security_count],
          global_index % 2 == 0 ? "Buy" : "Sell",
          static_cast<unsigned int>(global_index % 1000 + 1),
          "ConcurrentUser" + std::to_string(global_index % 64),
          "ConcurrentCompany" + std::to_string(global_index % 11)});

      if (order_index % 128 == 0) {
        cache_.get_matching_size_for_security(security_ids[global_index % security_count]);
        cache_.get_all_orders();
      }
    }
  });

  const std::vector<order_t> all_orders = cache_.get_all_orders();
  ASSERT_EQ(all_orders.size(), thread_count * orders_per_thread);
  expect_matching_equals_reference(cache_, security_ids, all_orders);
}

TEST_F(concurrency_test_t, concurrent_add_and_cancel_leaves_exact_expected_orders)
{
  constexpr std::size_t thread_count = 8;
  constexpr std::size_t orders_per_thread = 1200;
  constexpr unsigned int minimum_quantity = 80;

  std::vector<std::string> security_ids;
  security_ids.reserve(thread_count);
  for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
    security_ids.push_back("MixedSecurity" + std::to_string(thread_index));
  }

  const auto make_order = [&](std::size_t thread_index_p, std::size_t order_index_p) {
    const std::string thread_suffix = std::to_string(thread_index_p);
    return order_t{
        "MixedOrder" + thread_suffix + "_" + std::to_string(order_index_p),
        security_ids[thread_index_p],
        order_index_p % 2 == 0 ? "Buy" : "Sell",
        static_cast<unsigned int>(order_index_p % 100 + 1),
        (order_index_p % 5 == 0 ? "CancelledUser" : "KeptUser") + thread_suffix,
        "MixedCompany" + std::to_string(order_index_p % 7)};
  };

  const auto is_kept = [&](std::size_t order_index_p) {
    return order_index_p % 7 != 0 && order_index_p % 5 != 0 && order_index_p % 100 + 1 < minimum_quantity;
  };

  run_in_parallel(thread_count, [&](std::size_t thread_index_p) {
    for (std::size_t order_index = 0; order_index < orders_per_thread; ++order_index) {
      cache_.add_order(make_order(thread_index_p, order_index));
    }
    for (std::size_t order_index = 0; order_index < orders_per_thread; order_index += 7) {
      cache_.cancel_order(make_order(thread_index_p, order_index).order_id());
    }
    cache_.cancel_orders_for_user("CancelledUser" + std::to_string(thread_index_p));
    cache_.cancel_orders_for_security_with_minimum_quantity(security_ids[thread_index_p], minimum_quantity);
  });

  std::vector<order_t> expected_orders;
  for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
    for (std::size_t order_index = 0; order_index < orders_per_thread; ++order_index) {
      if (is_kept(order_index)) {
        expected_orders.push_back(make_order(thread_index, order_index));
      }
    }
  }

  const std::vector<order_t> actual_orders = cache_.get_all_orders();
  expect_orders_equal(actual_orders, expected_orders);
  expect_matching_equals_reference(cache_, security_ids, actual_orders);
}

TEST_F(concurrency_test_t, concurrent_duplicate_id_allows_exactly_one_add)
{
  constexpr std::size_t thread_count = 8;
  std::atomic<std::size_t> successful_adds{0};
  std::atomic<std::size_t> invalid_adds{0};
  std::atomic<std::size_t> unexpected_failures{0};

  run_in_parallel(thread_count, [&](std::size_t thread_index_p) {
    try {
      cache_.add_order(order_t{
          "SharedOrder",
          "SharedSecurity",
          thread_index_p % 2 == 0 ? "Buy" : "Sell",
          static_cast<unsigned int>(thread_index_p + 1),
          "SharedUser" + std::to_string(thread_index_p),
          "SharedCompany" + std::to_string(thread_index_p)});
      successful_adds.fetch_add(1, std::memory_order_relaxed);
    } catch (const std::invalid_argument&) {
      invalid_adds.fetch_add(1, std::memory_order_relaxed);
    } catch (...) {
      unexpected_failures.fetch_add(1, std::memory_order_relaxed);
    }
  });

  EXPECT_EQ(successful_adds.load(std::memory_order_relaxed), 1u);
  EXPECT_EQ(invalid_adds.load(std::memory_order_relaxed), thread_count - 1);
  EXPECT_EQ(unexpected_failures.load(std::memory_order_relaxed), 0u);
  const std::vector<order_t> orders = cache_.get_all_orders();
  ASSERT_EQ(orders.size(), 1u);
  EXPECT_EQ(orders.front().order_id(), "SharedOrder");
}

TEST_F(performance_test_t, handles_one_million_orders_efficiently)
{
  constexpr std::size_t order_count = 1000000;
  constexpr std::size_t security_count = 1000;
  constexpr std::size_t company_count = 100;
  constexpr std::size_t user_count = 10000;
  constexpr std::size_t users_to_cancel = 1000;
  constexpr unsigned int minimum_quantity = 5000;

  std::vector<std::string> security_ids;
  security_ids.reserve(security_count);
  for (std::size_t index = 0; index < security_count; ++index) {
    security_ids.push_back("Security" + std::to_string(index));
  }

  std::vector<std::string> companies;
  companies.reserve(company_count);
  for (std::size_t index = 0; index < company_count; ++index) {
    companies.push_back("Company" + std::to_string(index));
  }

  std::vector<std::string> users;
  users.reserve(user_count);
  for (std::size_t index = 0; index < user_count; ++index) {
    users.push_back("User" + std::to_string(index));
  }

  std::mt19937 random_engine(1729);
  std::uniform_int_distribution<std::size_t> security_distribution(0, security_count - 1);
  std::uniform_int_distribution<std::size_t> company_distribution(0, company_count - 1);
  std::uniform_int_distribution<std::size_t> user_distribution(0, user_count - 1);
  std::uniform_int_distribution<unsigned int> quantity_distribution(1, 10000);
  std::uniform_int_distribution<int> side_distribution(0, 1);

  std::vector<order_t> orders;
  orders.reserve(order_count);
  for (std::size_t index = 0; index < order_count; ++index) {
    orders.emplace_back(
        "Order" + std::to_string(index),
        security_ids[security_distribution(random_engine)],
        side_distribution(random_engine) == 0 ? "Buy" : "Sell",
        quantity_distribution(random_engine),
        users[user_distribution(random_engine)],
        companies[company_distribution(random_engine)]);
  }

  [[maybe_unused]] const auto measured_test_start = std::chrono::steady_clock::now();

  time_phase("add all orders", [&] {
    for (const order_t& order : orders) {
      cache_.add_order(order);
    }
  });

  std::vector<unsigned int> actual_matching(security_count);
  time_phase("getMatchingSizeForSecurity for every security", [&] {
    for (std::size_t index = 0; index < security_count; ++index) {
      actual_matching[index] = cache_.get_matching_size_for_security(security_ids[index]);
    }
  });

  std::vector<order_t> all_orders;
  time_phase("getAllOrders", [&] {
    all_orders = cache_.get_all_orders();
  });
  ASSERT_EQ(all_orders.size(), order_count);

  const auto expected_matching = calculate_reference_matching(all_orders);
  for (std::size_t index = 0; index < security_count; ++index) {
    EXPECT_EQ(actual_matching[index], expected_matching.at(security_ids[index])) << security_ids[index];
  }

  orders.clear();
  orders.shrink_to_fit();
  all_orders.clear();
  all_orders.shrink_to_fit();

  time_phase("cancelOrdersForUser for 1000 users", [&] {
    for (std::size_t index = 0; index < users_to_cancel; ++index) {
      cache_.cancel_orders_for_user(users[index]);
    }
  });

  time_phase("cancelOrdersForSecIdWithMinimumQty for every security", [&] {
    for (const std::string& security_id : security_ids) {
      cache_.cancel_orders_for_security_with_minimum_quantity(security_id, minimum_quantity);
    }
  });

  const std::vector<order_t> remaining_orders = cache_.get_all_orders();
  expect_matching_equals_reference(cache_, security_ids, remaining_orders);
  std::vector<std::string> remaining_order_ids;
  remaining_order_ids.reserve(remaining_orders.size());
  for (const order_t& order : remaining_orders) {
    remaining_order_ids.push_back(order.order_id());
  }

  time_phase("cancelOrder for every remaining order", [&] {
    for (const std::string& order_id : remaining_order_ids) {
      cache_.cancel_order(order_id);
    }
  });

  EXPECT_TRUE(cache_.get_all_orders().empty());

#ifdef NDEBUG
  const auto measured_test_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - measured_test_start);
  EXPECT_LT(measured_test_elapsed, std::chrono::seconds(30));
#endif
}

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
