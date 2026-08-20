#include "order_book.hpp"

#include <iostream>

int main() {
    lob::OrderBook book;

    book.add_order(lob::Order(1, lob::OrderSide::Buy, lob::OrderType::Limit, 100, 10, book.next_timestamp()));
    book.add_order(lob::Order(2, lob::OrderSide::Sell, lob::OrderType::Limit, 101, 5, book.next_timestamp()));

    auto trades = book.add_order(lob::Order(3, lob::OrderSide::Sell, lob::OrderType::Limit, 99, 4, book.next_timestamp()));

    std::cout << "Trades executed: " << trades.size() << "\n";
    for (const auto& t : trades) {
        std::cout << "  buy=" << t.buyId << " sell=" << t.sellId
                   << " price=" << t.price << " qty=" << t.qty << "\n";
    }

    if (auto bid = book.best_bid()) {
        std::cout << "Best bid: " << *bid << "\n";
    }
    if (auto ask = book.best_ask()) {
        std::cout << "Best ask: " << *ask << "\n";
    }

    return 0;
}
