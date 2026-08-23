#pragma once

#include "order.hpp"
#include "order_book.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace lob {

struct SimulatorConfig {
    Price tick_size = 1;
    Price initial_mid = 10000;

    Quantity min_order_qty = 1;
    Quantity max_order_qty = 100;

    double limit_order_weight = 1.0;
    double market_order_weight = 0.2;
    double cancel_weight = 0.5;

    int max_offset_ticks = 20;
    double offset_decay = 0.25;

    int initial_depth_levels = 10;

    std::size_t snapshot_interval = 1;

    std::uint64_t seed = 42;
};

struct BookSnapshot {
    Timestamp event_index;
    std::optional<Price> best_bid;
    std::optional<Price> best_ask;
};

struct SimulationStats {
    std::size_t num_limit_orders = 0;
    std::size_t num_market_orders = 0;
    std::size_t num_cancel_attempts = 0;
    std::size_t num_cancels = 0;
    std::size_t num_trades = 0;
    Quantity total_traded_qty = 0;

    std::vector<Trade> trades;
    std::vector<BookSnapshot> snapshots;
};

struct SummaryMetrics {
    std::size_t num_trades = 0;
    Quantity total_traded_qty = 0;
    double vwap = 0.0;
    std::optional<Price> final_best_bid;
    std::optional<Price> final_best_ask;
    std::optional<Price> final_spread;
};

SummaryMetrics summarize(const SimulationStats& stats, const OrderBook& book);

class Simulator {
public:
    explicit Simulator(SimulatorConfig config);

    void run(std::size_t num_events);

    OrderBook& book() { return book_; }
    const OrderBook& book() const { return book_; }
    const SimulationStats& stats() const { return stats_; }
    const SimulatorConfig& config() const { return config_; }

private:
    enum class EventKind { LimitOrder, MarketOrder, Cancel };

    SimulatorConfig config_;
    OrderBook book_;
    SimulationStats stats_;

    std::mt19937_64 rng_;
    std::discrete_distribution<int> event_dist_;
    std::bernoulli_distribution side_dist_;
    std::uniform_int_distribution<Quantity> qty_dist_;
    std::geometric_distribution<int> offset_dist_;

    std::vector<OrderId> live_order_ids_;
    OrderId next_order_id_ = 1;
    OrderId next_synthetic_id_ = -1;
    Timestamp event_index_ = 0;

    void seed_initial_book();

    EventKind sample_event_kind();
    OrderSide sample_side();
    Quantity sample_qty();
    int sample_offset_ticks();
    Price sample_limit_price(OrderSide side);

    void generate_limit_order();
    void generate_market_order();
    void generate_cancel();

    void record_trades(const std::vector<Trade>& trades);
    void record_snapshot();
    void track_order(OrderId id);
};

}
