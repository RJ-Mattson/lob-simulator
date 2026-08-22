#pragma once

#include "order.hpp"

#include <map>
#include <unordered_map>
#include <deque>
#include <vector>
#include <optional>
#include <functional>
#include <cstdint>

namespace lob {
    struct PriceLevel{
        std::deque<Order>  orders;
        Quantity total_qty = 0;
    };

    struct OrderLocation{
        OrderSide side;
        Price price;
    };

    class OrderBook {
    public:
        OrderBook() = default;
        
        std::vector<Trade> add_order(Order order);

        void seed_resting_order(Order order);

        bool cancel_order(OrderId id);

        bool modify_order(OrderId id, Quantity new_qty);

        std::optional<Price> best_bid() const;
        std::optional<Price> best_ask() const;
        std::optional<Price> spread() const;

        Quantity depth_at(OrderSide side, Price price) const;

        std::vector<std::pair<Price, Quantity>> top_levels(OrderSide side, std::size_t n) const;


        Timestamp next_timestamp() {
            return sequence_++;
        }
    
    private:
        Timestamp sequence_ = 0;

        std::map<Price, PriceLevel, std::greater<Price>> bids_;
        std::map<Price, PriceLevel, std::less<Price>> asks_;

        std::unordered_map<OrderId, OrderLocation> locations_;

        std::vector<Trade> match(Order& incoming);

        void insert_resting_bid(Order order);
        void insert_resting_ask(Order order);
        
        bool remove_from_bid(Price price, OrderId id);
        bool remove_from_ask(Price price, OrderId id);
        
    
    };


}