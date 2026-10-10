#include "order_cache.h"

#include <iterator>
#include <stdexcept>

void order_cache_t::add_order(order_t order_p)
{
  const std::string order_id = order_p.order_id();
  const std::string security_id = order_p.security_id();
  const std::string side = order_p.side();
  const unsigned int quantity = order_p.quantity();
  const std::string user = order_p.user();
  const std::string company = order_p.company();

  if (order_id.empty()) {
    throw std::invalid_argument("order id must not be empty");
  }
  if (security_id.empty()) {
    throw std::invalid_argument("security id must not be empty");
  }
  if (user.empty()) {
    throw std::invalid_argument("user must not be empty");
  }
  if (company.empty()) {
    throw std::invalid_argument("company must not be empty");
  }
  const side_t parsed_side = parse_side(side);
  if (quantity == 0) {
    throw std::invalid_argument("quantity must be greater than zero");
  }
  if (orders_.find(order_id) != orders_.end()) {
    throw std::invalid_argument("order id already exists");
  }

  orders_.emplace(order_id, std::move(order_p));
  user_index_.add(user, order_id);
  auto& security_book = securities_[security_id];
  security_book.orders_by_qty_.emplace(quantity, order_id);
  change_volume(security_book, company, parsed_side, quantity, volume_change_t::add);
}

void order_cache_t::cancel_order(const std::string& order_id_p)
{
  remove_order(order_id_p);
}

void order_cache_t::cancel_orders_for_user(const std::string& user_p)
{
  const auto* order_ids = user_index_.find(user_p);
  if (order_ids == nullptr) {
    return;
  }

  const std::vector<std::string> order_ids_to_remove(order_ids->begin(), order_ids->end());
  for (const std::string& order_id : order_ids_to_remove) {
    remove_order(order_id);
  }
}

void order_cache_t::cancel_orders_for_security_with_minimum_quantity(
    const std::string& security_id_p,
    unsigned int minimum_quantity_p)
{
  auto security_iterator = securities_.find(security_id_p);
  if (security_iterator == securities_.end()) {
    return;
  }

  const auto first_order = security_iterator->second.orders_by_qty_.lower_bound(
      std::make_pair(minimum_quantity_p, std::string{}));

  std::vector<std::string> order_ids_to_remove;
  order_ids_to_remove.reserve(
      static_cast<std::size_t>(std::distance(first_order, security_iterator->second.orders_by_qty_.end())));

  for (auto order_iterator = first_order;
       order_iterator != security_iterator->second.orders_by_qty_.end();
       ++order_iterator) {
    order_ids_to_remove.push_back(order_iterator->second);
  }

  for (const std::string& order_id : order_ids_to_remove) {
    remove_order(order_id);
  }
}

unsigned int order_cache_t::get_matching_size_for_security(const std::string&)
{
  return 0;
}

std::vector<order_t> order_cache_t::get_all_orders() const
{
  std::vector<order_t> orders;
  orders.reserve(orders_.size());
  for (const auto& order_entry : orders_) {
    orders.push_back(order_entry.second);
  }
  return orders;
}

order_cache_t::side_t order_cache_t::parse_side(const std::string& side_p)
{
  if (side_p == "Buy") {
    return side_t::buy;
  }
  if (side_p == "Sell") {
    return side_t::sell;
  }
  throw std::invalid_argument("side must be Buy or Sell");
}

void order_cache_t::apply_change(
    std::uint64_t& value_p,
    std::uint64_t quantity_p,
    volume_change_t change_p)
{
  if (change_p == volume_change_t::add) {
    value_p += quantity_p;
  } else {
    value_p -= quantity_p;
  }
}

void order_cache_t::change_volume(
    security_book_t& security_book_p,
    const std::string& company_p,
    side_t side_p,
    std::uint64_t quantity_p,
    volume_change_t change_p)
{
  auto [company_iterator, company_inserted] = security_book_p.companies_.try_emplace(company_p);
  company_volume_t& company_volume = company_iterator->second;
  if (!company_inserted) {
    const std::uint64_t old_total = company_volume.buy_ + company_volume.sell_;
    auto old_total_iterator = security_book_p.company_totals_.find(old_total);
    security_book_p.company_totals_.erase(old_total_iterator);
  }

  std::uint64_t& company_volume_for_side = side_p == side_t::buy ? company_volume.buy_ : company_volume.sell_;
  std::uint64_t& security_total_for_side =
      side_p == side_t::buy ? security_book_p.buy_total_ : security_book_p.sell_total_;
  apply_change(company_volume_for_side, quantity_p, change_p);
  apply_change(security_total_for_side, quantity_p, change_p);

  const std::uint64_t new_total = company_volume.buy_ + company_volume.sell_;
  if (new_total == 0) {
    security_book_p.companies_.erase(company_iterator);
  } else {
    security_book_p.company_totals_.insert(new_total);
  }
}

void order_cache_t::remove_order(const std::string& order_id_p)
{
  auto order_iterator = orders_.find(order_id_p);
  if (order_iterator == orders_.end()) {
    return;
  }

  const std::string order_id = order_iterator->second.order_id();
  const std::string security_id = order_iterator->second.security_id();
  const std::string side = order_iterator->second.side();
  const unsigned int quantity = order_iterator->second.quantity();
  const std::string user = order_iterator->second.user();
  const std::string company = order_iterator->second.company();
  const side_t parsed_side = parse_side(side);

  auto security_iterator = securities_.find(security_id);
  const auto order_quantity = std::make_pair(quantity, order_id);
  auto quantity_iterator = security_iterator->second.orders_by_qty_.find(order_quantity);

  change_volume(
      security_iterator->second,
      company,
      parsed_side,
      quantity,
      volume_change_t::remove);

  user_index_.remove(user, order_id);
  security_iterator->second.orders_by_qty_.erase(quantity_iterator);
  orders_.erase(order_iterator);

  if (security_iterator->second.orders_by_qty_.empty()) {
    securities_.erase(security_iterator);
  }
}
