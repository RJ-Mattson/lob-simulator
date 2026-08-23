#include "simulator.hpp"

#include <cassert>
#include <iostream>
#include <optional>

namespace {

int failures = 0;

void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << "\n";
        ++failures;
    }
}

void test_seeded_book_has_two_sided_depth() {
    lob::SimulatorConfig config;
    config.seed = 1;
    lob::Simulator sim(config);

    check(sim.book().best_bid().has_value(), "seeded book should have a best bid");
    check(sim.book().best_ask().has_value(), "seeded book should have a best ask");
    check(*sim.book().best_bid() < *sim.book().best_ask(), "seeded book should not be crossed");
}

void test_run_produces_events_and_valid_book() {
    lob::SimulatorConfig config;
    config.seed = 7;
    lob::Simulator sim(config);
    sim.run(2000);

    const auto& stats = sim.stats();
    check(stats.num_limit_orders + stats.num_market_orders + stats.num_cancel_attempts > 0,
          "run should generate events");
    check(stats.num_limit_orders > 0, "run should generate at least one limit order");

    if (auto bid = sim.book().best_bid()) {
        if (auto ask = sim.book().best_ask()) {
            check(*bid < *ask, "final book should never be crossed");
        }
    }

    for (const auto& t : stats.trades) {
        check(t.qty > 0, "recorded trades should have positive quantity");
    }
}

void test_deterministic_given_same_seed() {
    lob::SimulatorConfig config;
    config.seed = 123;

    lob::Simulator sim_a(config);
    sim_a.run(500);

    lob::Simulator sim_b(config);
    sim_b.run(500);

    const auto& stats_a = sim_a.stats();
    const auto& stats_b = sim_b.stats();

    check(stats_a.num_trades == stats_b.num_trades, "same seed should produce same trade count");
    check(stats_a.total_traded_qty == stats_b.total_traded_qty,
          "same seed should produce same traded quantity");
    check(sim_a.book().best_bid() == sim_b.book().best_bid(),
          "same seed should produce same final best bid");
    check(sim_a.book().best_ask() == sim_b.book().best_ask(),
          "same seed should produce same final best ask");
}

void test_different_seeds_diverge() {
    lob::SimulatorConfig config_a;
    config_a.seed = 1;
    lob::Simulator sim_a(config_a);
    sim_a.run(1000);

    lob::SimulatorConfig config_b;
    config_b.seed = 2;
    lob::Simulator sim_b(config_b);
    sim_b.run(1000);

    const auto& stats_a = sim_a.stats();
    const auto& stats_b = sim_b.stats();

    bool identical = stats_a.num_trades == stats_b.num_trades &&
                      stats_a.total_traded_qty == stats_b.total_traded_qty &&
                      sim_a.book().best_bid() == sim_b.book().best_bid();
    check(!identical, "different seeds should very likely produce different outcomes");
}

void test_cancel_only_removes_live_orders() {
    lob::SimulatorConfig config;
    config.seed = 5;
    config.limit_order_weight = 0.0;
    config.market_order_weight = 0.0;
    config.cancel_weight = 1.0;
    config.initial_depth_levels = 100;

    lob::Simulator sim(config);
    sim.run(50);

    const auto& stats = sim.stats();
    check(stats.num_cancel_attempts == 50, "every event should be a cancel attempt");
    check(stats.num_cancels <= stats.num_cancel_attempts,
          "successful cancels cannot exceed attempts");
}

void test_snapshot_interval_controls_snapshot_count() {
    lob::SimulatorConfig config;
    config.seed = 9;
    config.snapshot_interval = 10;

    lob::Simulator sim(config);
    sim.run(100);

    check(sim.stats().snapshots.size() == 10, "snapshots should be taken every 10th event");
}

}

int main() {
    test_seeded_book_has_two_sided_depth();
    test_run_produces_events_and_valid_book();
    test_deterministic_given_same_seed();
    test_different_seeds_diverge();
    test_cancel_only_removes_live_orders();
    test_snapshot_interval_controls_snapshot_count();

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
