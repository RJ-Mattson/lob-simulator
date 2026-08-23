#include "sweep.hpp"

namespace lob {

std::vector<SimulatorConfig> expand_grid(const SimulatorConfig& base, const std::vector<SweepAxis>& axes) {
    std::vector<SimulatorConfig> configs = {base};

    for (const auto& axis : axes) {
        std::vector<SimulatorConfig> next;
        next.reserve(configs.size() * axis.num_values);
        for (const auto& config : configs) {
            for (std::size_t i = 0; i < axis.num_values; ++i) {
                SimulatorConfig variant = config;
                axis.apply(variant, i);
                next.push_back(variant);
            }
        }
        configs = std::move(next);
    }

    return configs;
}

}
