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
    lob::OrderId id;
    OrderSide side;
    OrderType type;
    lob::Price price;
    lob::Quantity qty;
    lob::Timestamp time;

    Order(lob::OrderId id_, OrderSide side_, OrderType type_, lob::Price price_, lob::Quantity qty_, lob::Timestamp time_) :
    id(id_), side(side_), type(type_), price(price_), qty(qty_), time(time_) {}
};

struct Trade{
    lob::OrderId buyId;
    lob::OrderId sellId;
    lob::Price price;
    lob::Quantity qty;
    lob::Timestamp time;

    Trade(lob::OrderId buyId_, lob::OrderId sellId_, lob::Price price_, lob::Quantity qty_, lob::Timestamp time_) :
    buyId(buyId_), sellId(sellId_), price(price_), qty(qty_), time(time_){}
};
}