#include "order_book.hpp"

#include <algorithm>

namespace lob {

std::vector<Trade> OrderBook::add_order(Order order) {
    std::vector<Trade> trades = match(order);

    if (order.type == OrderType::Market || order.type == OrderType::IOC) {
        return trades;
    }

    if (order.qty > 0) {
        if (order.side == OrderSide::Buy) {
            insert_resting_bid(order);
        } else {
            insert_resting_ask(order);
        }
    }

    return trades;
}

std::vector<Trade> OrderBook::match(Order& incoming) {
    std::vector<Trade> trades;
    const bool incoming_is_buy = (incoming.side == OrderSide::Buy);

    // Walks the opposite side's price levels (best price first, since both
    // bids_ and asks_ iterate best-to-worst under their respective
    // comparators) draining resting orders FIFO until the incoming order is
    // filled or no more levels cross.
    auto walk = [&](auto& opposite_book) {
        while (incoming.qty > 0 && !opposite_book.empty()) {
            auto level_it = opposite_book.begin();
            Price level_price = level_it->first;

            if (incoming.type == OrderType::Limit) {
                bool crosses = incoming_is_buy ? (incoming.price >= level_price)
                                                : (incoming.price <= level_price);
                if (!crosses) {
                    break;
                }
            }

            PriceLevel& level = level_it->second;

            while (incoming.qty > 0 && !level.orders.empty()) {
                Order& resting = level.orders.front();
                Quantity traded_qty = std::min(incoming.qty, resting.qty);

                OrderId buy_id = incoming_is_buy ? incoming.id : resting.id;
                OrderId sell_id = incoming_is_buy ? resting.id : incoming.id;
                trades.emplace_back(buy_id, sell_id, level_price, traded_qty, next_timestamp());

                incoming.qty -= traded_qty;
                resting.qty -= traded_qty;
                level.total_qty -= traded_qty;

                if (resting.qty == 0) {
                    locations_.erase(resting.id);
                    level.orders.pop_front();
                }
            }

            if (level.orders.empty()) {
                opposite_book.erase(level_it);
            }
        }
    };

    if (incoming_is_buy) {
        walk(asks_);
    } else {
        walk(bids_);
    }

    return trades;
}

void OrderBook::insert_resting_bid(Order order) {
    OrderId id = order.id;
    Price price = order.price;
    PriceLevel& level = bids_[price];
    level.total_qty += order.qty;
    level.orders.push_back(order);
    locations_[id] = OrderLocation{OrderSide::Buy, price};
}

void OrderBook::insert_resting_ask(Order order) {
    OrderId id = order.id;
    Price price = order.price;
    PriceLevel& level = asks_[price];
    level.total_qty += order.qty;
    level.orders.push_back(order);
    locations_[id] = OrderLocation{OrderSide::Sell, price};
}

bool OrderBook::remove_from_bid(Price price, OrderId id) {
    auto level_it = bids_.find(price);
    if (level_it == bids_.end()) {
        return false;
    }

    PriceLevel& level = level_it->second;
    for (auto it = level.orders.begin(); it != level.orders.end(); ++it) {
        if (it->id == id) {
            level.total_qty -= it->qty;
            level.orders.erase(it);
            if (level.orders.empty()) {
                bids_.erase(level_it);
            }
            return true;
        }
    }
    return false;
}

bool OrderBook::remove_from_ask(Price price, OrderId id) {
    auto level_it = asks_.find(price);
    if (level_it == asks_.end()) {
        return false;
    }

    PriceLevel& level = level_it->second;
    for (auto it = level.orders.begin(); it != level.orders.end(); ++it) {
        if (it->id == id) {
            level.total_qty -= it->qty;
            level.orders.erase(it);
            if (level.orders.empty()) {
                asks_.erase(level_it);
            }
            return true;
        }
    }
    return false;
}

bool OrderBook::cancel_order(OrderId id) {
    auto loc_it = locations_.find(id);
    if (loc_it == locations_.end()) {
        return false;
    }

    OrderLocation loc = loc_it->second;
    locations_.erase(loc_it);

    if (loc.side == OrderSide::Buy) {
        return remove_from_bid(loc.price, id);
    }
    return remove_from_ask(loc.price, id);
}

bool OrderBook::modify_order(OrderId id, Quantity new_qty) {
    auto loc_it = locations_.find(id);
    if (loc_it == locations_.end()) {
        return false;
    }

    OrderLocation loc = loc_it->second;

    auto apply = [&](auto& book) {
        auto level_it = book.find(loc.price);
        if (level_it == book.end()) {
            return false;
        }

        PriceLevel& level = level_it->second;
        for (auto it = level.orders.begin(); it != level.orders.end(); ++it) {
            if (it->id != id) {
                continue;
            }

            if (new_qty <= 0) {
                level.total_qty -= it->qty;
                level.orders.erase(it);
                if (level.orders.empty()) {
                    book.erase(level_it);
                }
                locations_.erase(loc_it);
                return true;
            }

            if (new_qty > it->qty) {
                // Increasing quantity loses time priority: remove and
                // re-insert at the back of the level.
                Order updated = *it;
                updated.qty = new_qty;
                level.total_qty -= it->qty;
                level.orders.erase(it);
                if (level.orders.empty()) {
                    book.erase(level_it);
                }
                if (loc.side == OrderSide::Buy) {
                    insert_resting_bid(updated);
                } else {
                    insert_resting_ask(updated);
                }
            } else {
                level.total_qty -= (it->qty - new_qty);
                it->qty = new_qty;
            }
            return true;
        }
        return false;
    };

    if (loc.side == OrderSide::Buy) {
        return apply(bids_);
    }
    return apply(asks_);
}

std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }
    return asks_.begin()->first;
}

std::optional<Price> OrderBook::spread() const {
    auto bid = best_bid();
    auto ask = best_ask();
    if (!bid || !ask) {
        return std::nullopt;
    }
    return *ask - *bid;
}

Quantity OrderBook::depth_at(OrderSide side, Price price) const {
    if (side == OrderSide::Buy) {
        auto it = bids_.find(price);
        return it == bids_.end() ? 0 : it->second.total_qty;
    }
    auto it = asks_.find(price);
    return it == asks_.end() ? 0 : it->second.total_qty;
}

}  // namespace lob
