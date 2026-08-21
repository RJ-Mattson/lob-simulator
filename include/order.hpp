#pragma once

#include <cstdint>


namespace lob {
    using OrderId = std::int64_t;
    using Price = std::int64_t;
    using Quantity = std::int64_t;
    using Timestamp = std::int64_t;


enum class OrderSide {
    Buy,
    Sell
};

enum class OrderType {
    Limit,
    Market,
    IOC
};

struct Order{
    OrderId id;
    OrderSide side;
    OrderType type;
    Price price;
    Quantity qty;
    Timestamp time;

    Order(OrderId id_, OrderSide side_, OrderType type_, Price price_, Quantity qty_, Timestamp time_) :
    id(id_), side(side_), type(type_), price(price_), qty(qty_), time(time_) {}
};

struct Trade{
    OrderId buyId;
    OrderId sellId;
    Price price;
    Quantity qty;
    Timestamp time;

    Trade(OrderId buyId_, OrderId sellId_, Price price_, Quantity qty_, Timestamp time_) :
    buyId(buyId_), sellId(sellId_), price(price_), qty(qty_), time(time_){}
};
}