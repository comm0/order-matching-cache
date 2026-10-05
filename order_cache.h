#pragma once

#include <string>
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

};
