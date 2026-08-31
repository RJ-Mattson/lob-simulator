#include "simulator.hpp"

#include <algorithm>

namespace lob {

Simulator::Simulator(SimulatorConfig config)
    : config_(config),
      rng_(config.seed),
      side_dist_(0.5) {
    apply_regime(config_);
    seed_initial_book();
}

void Simulator::apply_regime(const SimulatorConfig& config) {
    config_ = config;
    event_dist_ = std::discrete_distribution<int>(
        {config_.limit_order_weight, config_.market_order_weight, config_.cancel_weight});
    qty_dist_ = std::uniform_int_distribution<Quantity>(config_.min_order_qty, config_.max_order_qty);
    offset_dist_ = std::geometric_distribution<int>(config_.offset_decay);
}

void Simulator::schedule_regime_change(std::size_t at_event, SimulatorConfig config) {
    RegimeChange change{at_event, std::move(config)};
    auto it = std::upper_bound(
        regime_changes_.begin(), regime_changes_.end(), change,
        [](const RegimeChange& a, const RegimeChange& b) { return a.at_event < b.at_event; });
    regime_changes_.insert(it, std::move(change));
}

void Simulator::seed_initial_book() {
    for (int level = 1; level <= config_.initial_depth_levels; ++level) {
        Price bid_price = config_.initial_mid - static_cast<Price>(level) * config_.tick_size;
        Order bid(next_synthetic_id_--, OrderSide::Buy, OrderType::Limit, bid_price, sample_qty(), book_.next_timestamp());
        book_.seed_resting_order(bid);
        track_order(bid.id);

        Price ask_price = config_.initial_mid + static_cast<Price>(level) * config_.tick_size;
        Order ask(next_synthetic_id_--, OrderSide::Sell, OrderType::Limit, ask_price, sample_qty(), book_.next_timestamp());
        book_.seed_resting_order(ask);
        track_order(ask.id);
    }
}

void Simulator::run(std::size_t num_events) {
    for (std::size_t i = 0; i < num_events; ++i) {
        while (next_regime_index_ < regime_changes_.size() &&
               regime_changes_[next_regime_index_].at_event <= static_cast<std::size_t>(event_index_)) {
            apply_regime(regime_changes_[next_regime_index_].config);
            ++next_regime_index_;
        }

        switch (sample_event_kind()) {
            case EventKind::LimitOrder:
                generate_limit_order();
                break;
            case EventKind::MarketOrder:
                generate_market_order();
                break;
            case EventKind::Cancel:
                generate_cancel();
                break;
        }

        ++event_index_;
        if (config_.snapshot_interval > 0 && event_index_ % config_.snapshot_interval == 0) {
            record_snapshot();
        }
    }
}

Simulator::EventKind Simulator::sample_event_kind() {
    return static_cast<EventKind>(event_dist_(rng_));
}

OrderSide Simulator::sample_side() {
    return side_dist_(rng_) ? OrderSide::Buy : OrderSide::Sell;
}

Quantity Simulator::sample_qty() {
    return qty_dist_(rng_);
}

int Simulator::sample_offset_ticks() {
    int offset = offset_dist_(rng_);
    return offset > config_.max_offset_ticks ? config_.max_offset_ticks : offset;
}

Price Simulator::sample_limit_price(OrderSide side) {
    int offset = sample_offset_ticks();
    if (side == OrderSide::Buy) {
        Price reference = book_.best_ask().value_or(config_.initial_mid);
        return reference - static_cast<Price>(offset) * config_.tick_size;
    }
    Price reference = book_.best_bid().value_or(config_.initial_mid);
    return reference + static_cast<Price>(offset) * config_.tick_size;
}

void Simulator::generate_limit_order() {
    OrderSide side = sample_side();
    Quantity qty = sample_qty();
    Price price = sample_limit_price(side);
    OrderId id = next_order_id_++;

    Order order(id, side, OrderType::Limit, price, qty, book_.next_timestamp());
    auto trades = book_.add_order(order);
    record_trades(trades);
    ++stats_.num_limit_orders;

    Quantity matched = 0;
    for (const auto& t : trades) {
        if (side == OrderSide::Buy && t.buyId == id) {
            matched += t.qty;
        } else if (side == OrderSide::Sell && t.sellId == id) {
            matched += t.qty;
        }
    }

    if (qty - matched > 0) {
        track_order(id);
    }
}

void Simulator::generate_market_order() {
    OrderSide side = sample_side();
    Quantity qty = sample_qty();
    OrderId id = next_order_id_++;

    Order order(id, side, OrderType::Market, 0, qty, book_.next_timestamp());
    auto trades = book_.add_order(order);
    record_trades(trades);
    ++stats_.num_market_orders;
}

void Simulator::generate_cancel() {
    if (live_order_ids_.empty()) {
        generate_limit_order();
        return;
    }

    std::uniform_int_distribution<std::size_t> idx_dist(0, live_order_ids_.size() - 1);
    std::size_t idx = idx_dist(rng_);
    OrderId id = live_order_ids_[idx];
    live_order_ids_[idx] = live_order_ids_.back();
    live_order_ids_.pop_back();

    ++stats_.num_cancel_attempts;
    if (book_.cancel_order(id)) {
        ++stats_.num_cancels;
    }
}

void Simulator::record_trades(const std::vector<Trade>& trades) {
    for (const auto& t : trades) {
        stats_.trades.push_back(t);
        ++stats_.num_trades;
        stats_.total_traded_qty += t.qty;
    }
}

void Simulator::record_snapshot() {
    stats_.snapshots.push_back(BookSnapshot{event_index_, book_.best_bid(), book_.best_ask()});
}

void Simulator::track_order(OrderId id) {
    live_order_ids_.push_back(id);
}

SummaryMetrics summarize(const SimulationStats& stats, const OrderBook& book) {
    SummaryMetrics metrics;
    metrics.num_trades = stats.num_trades;
    metrics.total_traded_qty = stats.total_traded_qty;

    if (metrics.total_traded_qty > 0) {
        double notional = 0.0;
        for (const auto& t : stats.trades) {
            notional += static_cast<double>(t.price) * static_cast<double>(t.qty);
        }
        metrics.vwap = notional / static_cast<double>(metrics.total_traded_qty);
    }

    metrics.final_best_bid = book.best_bid();
    metrics.final_best_ask = book.best_ask();
    metrics.final_spread = book.spread();

    return metrics;
}

}
