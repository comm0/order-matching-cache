# Order Matching Cache

C++17 in-memory cache of buy/sell orders that answers "how much of this security can be traded right now" in O(1).

## Problem

- an order has: id, security id, side (`Buy` / `Sell`), quantity, user, company
- operations:
  - `add_order`, `cancel_order`, `cancel_orders_for_user`
  - `cancel_orders_for_security_with_minimum_quantity` - removes orders of a security with quantity >= the given minimum
  - `get_all_orders`
  - `get_matching_size_for_security` - total quantity that can be matched for a security
- matching rules:
  - a buy matches a sell of the same security
  - orders of the same company never match each other
  - one order can be split across many orders on the other side, every unit counts once
  - the answer is the maximum possible matched quantity
- design goals: standard library only (Google Test for tests), Linux and Windows, a million orders, strict input validation, safe concurrent use

## Design

Data structures:

- `orders_` - order id -> order, O(1) lookup and duplicate check
- `user_index_` - user -> order ids
- `securities_` - security id -> book:
  - buy total, sell total
  - buy / sell volume per company
  - multiset of per company totals, max is `rbegin()`
  - set of (quantity, order id), `lower_bound(minimum)` gives the range to cancel
- indexes keep ids, no pointers; every cancel goes through one `remove_order`
- sums are `uint64_t`

Matching never pairs individual orders, only volumes per company matter:

```
B = total buy, S = total sell, C_c = buy + sell of company c

matching = min(B, S, B + S - max_c(C_c))
```

- nothing can match beyond B or S
- every matched unit involves two distinct companies, so at least one side comes from outside the largest company
- the bound is always reachable (max-flow / min-cut); a test compares it with a brute-force max-flow over random orders
- greedy pairing order by order is not enough, its result depends on the pairing order (`order_dependent_greedy_trap` test)
- results above `UINT_MAX` are saturated

| Operation | Cost |
|---|---|
| `add_order`, `cancel_order` | O(log n) |
| `cancel_orders_for_user` | O(k log n) |
| `cancel_orders_for_security_with_minimum_quantity` | O(log n + k log n) |
| `get_matching_size_for_security` | O(1) |
| `get_all_orders` | O(n) |

Decisions:

- invalid input throws `std::invalid_argument` (empty fields, side other than exactly `Buy` / `Sell`, zero quantity, duplicate id) and leaves the cache unchanged
- cancelling something that does not exist is a no-op
- one `std::shared_mutex`: writes exclusive, reads shared, every public call is atomic

## Tests

Google Test, grouped into suites: storage, validation, cancellation, matching (hand-made scenarios, edge cases, brute-force max-flow on random data), concurrency, performance (1M orders).

## Build

Linux:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/order_cache_test
```

Windows: `vcpkg install gtest:x64-windows-static-md`, set `VCPKG_DIR` in `generate_vs2022.bat` and run it - it creates `vs/order_cache.sln`.

CI (GitHub Actions, manual trigger): GCC and Clang with `-Werror`, then ASan, UBSan and TSan runs.
