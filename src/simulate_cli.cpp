#include "simulator.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void print_usage(const char* prog) {
    std::cerr << "usage: " << prog
              << " [num_events] [seed] [--snapshots-csv path] [--trades-csv path]\n";
}

}

int main(int argc, char** argv) {
    std::size_t num_events = 10000;
    std::uint64_t seed = 42;
    std::string snapshots_csv_path;
    std::string trades_csv_path;

    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--snapshots-csv" && i + 1 < argc) {
            snapshots_csv_path = argv[++i];
        } else if (arg == "--trades-csv" && i + 1 < argc) {
            trades_csv_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            positional.push_back(arg);
        }
    }

    try {
        if (positional.size() > 0) {
            num_events = std::stoull(positional[0]);
        }
        if (positional.size() > 1) {
            seed = std::stoull(positional[1]);
        }
    } catch (const std::exception&) {
        print_usage(argv[0]);
        return 1;
    }

    lob::SimulatorConfig config;
    config.seed = seed;

    lob::Simulator sim(config);
    sim.run(num_events);

    const auto& stats = sim.stats();

    std::cout << "events requested: " << num_events << " (seed " << seed << ")\n";
    std::cout << "limit orders: " << stats.num_limit_orders << "\n";
    std::cout << "market orders: " << stats.num_market_orders << "\n";
    std::cout << "cancel attempts: " << stats.num_cancel_attempts
               << " (succeeded: " << stats.num_cancels << ")\n";
    std::cout << "trades: " << stats.num_trades << "\n";
    std::cout << "total traded qty: " << stats.total_traded_qty << "\n";

    lob::SummaryMetrics metrics = lob::summarize(stats, sim.book());
    if (metrics.total_traded_qty > 0) {
        std::cout << "vwap: " << metrics.vwap << "\n";
    }
    if (metrics.final_best_bid) {
        std::cout << "final best bid: " << *metrics.final_best_bid << "\n";
    }
    if (metrics.final_best_ask) {
        std::cout << "final best ask: " << *metrics.final_best_ask << "\n";
    }
    if (metrics.final_spread) {
        std::cout << "final spread: " << *metrics.final_spread << "\n";
    }

    if (!snapshots_csv_path.empty()) {
        std::ofstream out(snapshots_csv_path);
        if (!out) {
            std::cerr << "error: could not open " << snapshots_csv_path << " for writing\n";
            return 1;
        }
        out << "event_index,best_bid,best_ask\n";
        for (const auto& snap : stats.snapshots) {
            out << snap.event_index << ","
                << (snap.best_bid ? std::to_string(*snap.best_bid) : "") << ","
                << (snap.best_ask ? std::to_string(*snap.best_ask) : "") << "\n";
        }
    }

    if (!trades_csv_path.empty()) {
        std::ofstream out(trades_csv_path);
        if (!out) {
            std::cerr << "error: could not open " << trades_csv_path << " for writing\n";
            return 1;
        }
        out << "buy_id,sell_id,price,qty,time\n";
        for (const auto& t : stats.trades) {
            out << t.buyId << "," << t.sellId << "," << t.price << "," << t.qty << "," << t.time << "\n";
        }
    }

    return 0;
}
