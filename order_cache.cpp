#include "order_cache.h"

void order_cache_t::add_order(order_t order_p) {
}

void order_cache_t::cancel_order(const std::string& order_id_p) {
}

void order_cache_t::cancel_orders_for_user(const std::string& user_p) {
}

void order_cache_t::cancel_orders_for_security_with_minimum_quantity(const std::string& security_id_p, unsigned int minimum_quantity_p) {
}

unsigned int order_cache_t::get_matching_size_for_security(const std::string& security_id_p) {
}

std::vector<order_t> order_cache_t::get_all_orders() const {
}
