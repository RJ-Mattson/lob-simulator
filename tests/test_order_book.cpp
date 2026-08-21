#include "order_book.hpp"

#include <cassert>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << "\n";
        ++failures;
    }
}

void test_resting_limit_orders_no_cross() {
    lob::OrderBook book;
    auto trades = book.add_order(lob::Order(1, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 10, book.next_timestamp()));
    check(trades.empty(), "resting buy should not trade");
    check(book.best_bid() == 100, "best bid should be 100");
    check(book.best_ask() == std::nullopt, "no asks yet");
    check(book.depth_at(lob::OrderSide::Buy, 100) == 10, "depth at 100 should be 10");
}

void test_simple_cross() {
    lob::OrderBook book;
    book.add_order(lob::Order(1, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 10, book.next_timestamp()));
    auto trades = book.add_order(lob::Order(2, lob::OrderSide::Sell, lob::OrderType::Limit, 100, 4, book.next_timestamp()));

    check(trades.size() == 1, "one trade should occur");
    if (!trades.empty()) {
        check(trades[0].price == 100, "trade price should be 100");
        check(trades[0].qty == 4, "trade qty should be 4");
        check(trades[0].buyId == 1, "trade buy id should be 1");
        check(trades[0].sellId == 2, "trade sell id should be 2");
    }
    check(book.depth_at(lob::OrderSide::Buy, 100) == 6, "remaining bid depth should be 6");
}

void test_price_time_priority() {
    lob::OrderBook book;
    book.add_order(lob::Order(1, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 5, book.next_timestamp()));
    book.add_order(lob::Order(2, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 5, book.next_timestamp()));

    auto trades = book.add_order(lob::Order(3, lob::OrderSide::Sell, lob::OrderType::Limit, 100, 5, book.next_timestamp()));

    check(trades.size() == 1, "one trade should occur against the older order");
    if (!trades.empty()) {
        check(trades[0].buyId == 1, "the earlier resting order should fill first");
    }
    check(book.depth_at(lob::OrderSide::Buy, 100) == 5, "second order should remain resting");
}

void test_partial_fill_multiple_levels() {
    lob::OrderBook book;
    book.add_order(lob::Order(1, lob::OrderSide::Sell, lob::OrderType::Limit, 101, 5, book.next_timestamp()));
    book.add_order(lob::Order(2, lob::OrderSide::Sell, lob::OrderType::Limit, 102, 5, book.next_timestamp()));

    auto trades = book.add_order(lob::Order(3, lob::OrderSide::Buy, lob::OrderType::Limit, 102, 8, book.next_timestamp()));

    check(trades.size() == 2, "should sweep two price levels");
    check(book.best_ask() == 102, "best ask should still be 102");
    check(book.depth_at(lob::OrderSide::Sell, 102) == 2, "remaining ask depth at 102 should be 2");
}

void test_cancel_order() {
    lob::OrderBook book;
    book.add_order(lob::Order(1, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 10, book.next_timestamp()));

    check(book.cancel_order(1), "cancel should succeed for resting order");
    check(book.best_bid() == std::nullopt, "book should be empty after cancel");
    check(!book.cancel_order(1), "cancel should fail for already-cancelled order");
    check(!book.cancel_order(999), "cancel should fail for unknown id");
}

void test_modify_order_decrease() {
    lob::OrderBook book;
    book.add_order(lob::Order(1, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 10, book.next_timestamp()));

    check(book.modify_order(1, 4), "modify should succeed");
    check(book.depth_at(lob::OrderSide::Buy, 100) == 4, "depth should reflect reduced quantity");
}

void test_modify_order_to_zero_cancels() {
    lob::OrderBook book;
    book.add_order(lob::Order(1, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 10, book.next_timestamp()));

    check(book.modify_order(1, 0), "modify to zero should succeed");
    check(book.best_bid() == std::nullopt, "order should be removed after zero-qty modify");
}

void test_market_order_does_not_rest() {
    lob::OrderBook book;
    book.add_order(lob::Order(1, lob::OrderSide::Sell, lob::OrderType::Limit, 101, 5, book.next_timestamp()));

    auto trades = book.add_order(lob::Order(2, lob::OrderSide::Buy, lob::OrderType::Market, 0, 10, book.next_timestamp()));

    check(trades.size() == 1, "market order should trade against available liquidity");
    check(trades[0].qty == 5, "trade should be limited by available ask quantity");
    check(book.best_bid() == std::nullopt, "unfilled market order remainder should not rest");
}

void test_ioc_order_does_not_rest() {
    lob::OrderBook book;
    auto trades = book.add_order(lob::Order(1, lob::OrderSide::Buy, lob::OrderType::IOC, 100, 10, book.next_timestamp()));

    check(trades.empty(), "IOC with no liquidity should not trade");
    check(book.best_bid() == std::nullopt, "IOC order should never rest in the book");
}

void test_spread() {
    lob::OrderBook book;
    book.add_order(lob::Order(1, lob::OrderSide::Buy, lob::OrderType::Limit, 99, 5, book.next_timestamp()));
    book.add_order(lob::Order(2, lob::OrderSide::Sell, lob::OrderType::Limit, 101, 5, book.next_timestamp()));

    check(book.spread() == 2, "spread should be ask - bid");
}

void test_limit_order_does_not_cross() {
    lob::OrderBook book;
    book.add_order(lob::Order(1, lob::OrderSide::Sell, lob::OrderType::Limit, 101, 5, book.next_timestamp()));

    auto trades = book.add_order(lob::Order(2, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 5, book.next_timestamp()));

    check(trades.empty(), "buy priced below the best ask should not trade");
    check(book.best_bid() == 100, "non-crossing buy should rest instead");
    check(book.depth_at(lob::OrderSide::Sell, 101) == 5, "resting ask should be untouched");
}

void test_fifo_preserved_after_cancel() {
    lob::OrderBook book;
    book.add_order(lob::Order(1, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 5, book.next_timestamp()));
    book.add_order(lob::Order(2, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 5, book.next_timestamp()));
    book.add_order(lob::Order(3, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 5, book.next_timestamp()));

    check(book.cancel_order(2), "cancel of middle order should succeed");

    auto trades = book.add_order(lob::Order(4, lob::OrderSide::Sell, lob::OrderType::Limit, 100, 5, book.next_timestamp()));

    check(trades.size() == 1, "one trade should occur");
    if (!trades.empty()) {
        check(trades[0].buyId == 1, "the first surviving order should still fill first after a middle cancel");
    }
    check(book.depth_at(lob::OrderSide::Buy, 100) == 5, "order 3 should remain resting");
}

void test_modify_increase_loses_time_priority() {
    lob::OrderBook book;
    book.add_order(lob::Order(1, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 5, book.next_timestamp()));
    book.add_order(lob::Order(2, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 5, book.next_timestamp()));

    check(book.modify_order(1, 10), "increasing quantity should succeed");

    auto trades = book.add_order(lob::Order(3, lob::OrderSide::Sell, lob::OrderType::Limit, 100, 5, book.next_timestamp()));

    check(trades.size() == 1, "one trade should occur");
    if (!trades.empty()) {
        check(trades[0].buyId == 2, "order 2 should now fill first since order 1 lost time priority");
    }
    check(book.depth_at(lob::OrderSide::Buy, 100) == 10, "order 1's increased quantity should still be fully resting");
}

void test_market_order_exhausts_multiple_levels() {
    lob::OrderBook book;
    book.add_order(lob::Order(1, lob::OrderSide::Sell, lob::OrderType::Limit, 101, 5, book.next_timestamp()));
    book.add_order(lob::Order(2, lob::OrderSide::Sell, lob::OrderType::Limit, 102, 5, book.next_timestamp()));

    auto trades = book.add_order(lob::Order(3, lob::OrderSide::Buy, lob::OrderType::Market, 0, 20, book.next_timestamp()));

    check(trades.size() == 2, "market order should sweep both levels");
    lob::Quantity total_filled = 0;
    for (const auto& t : trades) {
        total_filled += t.qty;
    }
    check(total_filled == 10, "market order should only fill available liquidity, not the full requested qty");
    check(book.best_ask() == std::nullopt, "book should be fully drained on the ask side");
    check(book.best_bid() == std::nullopt, "unfilled remainder of the market order should not rest");
}

void test_zero_and_negative_qty_are_noop() {
    lob::OrderBook book;

    auto trades_zero = book.add_order(lob::Order(1, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 0, book.next_timestamp()));
    check(trades_zero.empty(), "zero-qty order should not trade");
    check(book.best_bid() == std::nullopt, "zero-qty order should not rest");

    auto trades_negative = book.add_order(lob::Order(2, lob::OrderSide::Buy, lob::OrderType::Limit, 100, -5, book.next_timestamp()));
    check(trades_negative.empty(), "negative-qty order should not trade");
    check(book.best_bid() == std::nullopt, "negative-qty order should not rest");
}

}

int main() {
    test_resting_limit_orders_no_cross();
    test_simple_cross();
    test_price_time_priority();
    test_partial_fill_multiple_levels();
    test_cancel_order();
    test_modify_order_decrease();
    test_modify_order_to_zero_cancels();
    test_market_order_does_not_rest();
    test_ioc_order_does_not_rest();
    test_spread();
    test_limit_order_does_not_cross();
    test_fifo_preserved_after_cancel();
    test_modify_increase_loses_time_priority();
    test_market_order_exhausts_multiple_levels();
    test_zero_and_negative_qty_are_noop();

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
