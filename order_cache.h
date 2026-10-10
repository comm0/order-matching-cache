#pragma once

#include <cstdint>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class order_t
{

 public:

  order_t(
      const std::string& order_id_p,
      const std::string& security_id_p,
      const std::string& side_p,
      const unsigned int quantity_p,
      const std::string& user_p,
      const std::string& company_p)
      : order_id_(order_id_p),
        security_id_(security_id_p),
        side_(side_p),
        quantity_(quantity_p),
        user_(user_p),
        company_(company_p) { }

  std::string order_id() const    { return order_id_; }
  std::string security_id() const { return security_id_; }
  std::string side() const        { return side_; }
  std::string user() const        { return user_; }
  std::string company() const     { return company_; }
  unsigned int quantity() const   { return quantity_; }

 private:

  std::string order_id_;     ///< Unique order identifier.
  std::string security_id_;  ///< Security identifier.
  std::string side_;         ///< Order side, e.g. Buy or Sell.
  unsigned int quantity_;    ///< Order quantity.
  std::string user_;         ///< Name of the user owning the order.
  std::string company_;      ///< Company of the user.

};

class order_cache_interface_t
{

 public:

  /// Adds an order to the cache.
  virtual void add_order(order_t order_p) = 0;

  /// Removes the order with the given unique id.
  virtual void cancel_order(const std::string& order_id_p) = 0;

  /// Removes all orders of the given user.
  virtual void cancel_orders_for_user(const std::string& user_p) = 0;

  /// Removes all orders of the given security with quantity >= minimum_quantity_p.
  virtual void cancel_orders_for_security_with_minimum_quantity(const std::string& security_id_p, unsigned int minimum_quantity_p) = 0;

  /// Returns the total quantity that can be matched for the given security.
  virtual unsigned int get_matching_size_for_security(const std::string& security_id_p) = 0;

  /// Returns all cached orders as a vector.
  virtual std::vector<order_t> get_all_orders() const = 0;

};

class order_cache_t : public order_cache_interface_t
{

 public:

  void add_order(order_t order_p) override;

  void cancel_order(const std::string& order_id_p) override;

  void cancel_orders_for_user(const std::string& user_p) override;

  void cancel_orders_for_security_with_minimum_quantity(const std::string& security_id_p, unsigned int minimum_quantity_p) override;

  unsigned int get_matching_size_for_security(const std::string& security_id_p) override;

  std::vector<order_t> get_all_orders() const override;

 private:

  template <typename key_t>
  class id_index_t
  {
   public:
    void add(const key_t& key_p, const std::string& order_id_p)
    {
      index_[key_p].insert(order_id_p);
    }

    void remove(const key_t& key_p, const std::string& order_id_p)
    {
      auto key_iterator = index_.find(key_p);
      if (key_iterator == index_.end()) {
        return;
      }

      key_iterator->second.erase(order_id_p);
      if (key_iterator->second.empty()) {
        index_.erase(key_iterator);
      }
    }

    const std::unordered_set<std::string>& find(const key_t& key_p) const
    {
      static const std::unordered_set<std::string> empty_set;
      const auto key_iterator = index_.find(key_p);
      return key_iterator == index_.end() ? empty_set : key_iterator->second;
    }

   private:
    std::unordered_map<key_t, std::unordered_set<std::string>> index_;
  };

  struct company_volume_t
  {
    std::uint64_t buy_ = 0;
    std::uint64_t sell_ = 0;
  };

  struct security_book_t
  {
    std::uint64_t buy_total_ = 0;
    std::uint64_t sell_total_ = 0;
    std::unordered_map<std::string, company_volume_t> companies_;
    std::multiset<std::uint64_t> company_totals_;
    std::set<std::pair<unsigned int, std::string>> orders_by_qty_;
  };

  enum class side_t
  {
    buy,
    sell
  };

  enum class volume_change_t
  {
    add,
    remove
  };

  static side_t parse_side(const std::string& side_p);

  static void apply_change(
      std::uint64_t& value_p,
      std::uint64_t quantity_p,
      volume_change_t change_p);

  static void change_volume(
      security_book_t& security_book_p,
      const std::string& company_p,
      side_t side_p,
      std::uint64_t quantity_p,
      volume_change_t change_p);

  void remove_order(const std::string& order_id_p);

  std::unordered_map<std::string, order_t> orders_;
  id_index_t<std::string> user_index_;
  std::unordered_map<std::string, security_book_t> securities_;
  mutable std::shared_mutex mutex_;
};
