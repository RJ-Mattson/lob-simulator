#include "simulator.hpp"
#include "sweep.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void print_usage(const char* prog) {
    std::cerr << "usage: " << prog << " [num_events] [--out path]\n";
    std::cerr << "  sweep axes and seeds are defined in main() — edit and rebuild to change the experiment\n";
}

}

int main(int argc, char** argv) {
    std::size_t num_events = 5000;
    std::string out_path = "experiment_results.csv";

    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--out" && i + 1 < argc) {
            out_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            positional.push_back(arg);
        }
    }

    try {
        if (!positional.empty()) {
            num_events = std::stoull(positional[0]);
        }
    } catch (const std::exception&) {
        print_usage(argv[0]);
        return 1;
    }

    // --- experiment definition: edit this block and rebuild to change the sweep ---
    lob::SimulatorConfig base;
    base.snapshot_interval = 0;  // grid runs only need final SummaryMetrics, not a full time series

    std::vector<lob::SweepAxis> axes = {
        lob::make_axis<double>("cancel_weight", &lob::SimulatorConfig::cancel_weight, {0.2, 0.4, 0.6}),
        lob::make_axis<double>("offset_decay", &lob::SimulatorConfig::offset_decay, {0.15, 0.25, 0.4}),
    };
    std::vector<std::uint64_t> seeds = {1, 2, 3, 4, 5};
    // --- end experiment definition ---

    std::vector<lob::SimulatorConfig> configs = lob::expand_grid(base, axes);

    std::ofstream out(out_path);
    if (!out) {
        std::cerr << "error: could not open " << out_path << " for writing\n";
        return 1;
    }

    out << "seed,tick_size,initial_mid,min_order_qty,max_order_qty,"
           "limit_order_weight,market_order_weight,cancel_weight,"
           "max_offset_ticks,offset_decay,initial_depth_levels,"
           "num_trades,total_traded_qty,vwap,final_best_bid,final_best_ask,final_spread\n";

    std::size_t total_runs = configs.size() * seeds.size();
    std::size_t run_index = 0;

    for (const auto& config_template : configs) {
        for (auto seed : seeds) {
            lob::SimulatorConfig config = config_template;
            config.seed = seed;

            lob::Simulator sim(config);
            sim.run(num_events);

            lob::SummaryMetrics metrics = lob::summarize(sim.stats(), sim.book());

            out << config.seed << ","
                << config.tick_size << ","
                << config.initial_mid << ","
                << config.min_order_qty << ","
                << config.max_order_qty << ","
                << config.limit_order_weight << ","
                << config.market_order_weight << ","
                << config.cancel_weight << ","
                << config.max_offset_ticks << ","
                << config.offset_decay << ","
                << config.initial_depth_levels << ","
                << metrics.num_trades << ","
                << metrics.total_traded_qty << ","
                << metrics.vwap << ","
                << (metrics.final_best_bid ? std::to_string(*metrics.final_best_bid) : "") << ","
                << (metrics.final_best_ask ? std::to_string(*metrics.final_best_ask) : "") << ","
                << (metrics.final_spread ? std::to_string(*metrics.final_spread) : "") << "\n";

            ++run_index;
            std::cerr << "\rrun " << run_index << "/" << total_runs << std::flush;
        }
    }
    std::cerr << "\n";

    std::cout << "wrote " << total_runs << " rows to " << out_path << "\n";
    return 0;
}
