#pragma once

#include "simulator.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace lob {

struct SweepAxis {
    std::string name;
    std::size_t num_values;
    std::function<void(SimulatorConfig&, std::size_t)> apply;
};

template <typename T>
SweepAxis make_axis(std::string name, T SimulatorConfig::*member, std::vector<T> values) {
    std::size_t num_values = values.size();
    return SweepAxis{
        std::move(name),
        num_values,
        [member, values = std::move(values)](SimulatorConfig& config, std::size_t index) {
            config.*member = values[index];
        }
    };
}

std::vector<SimulatorConfig> expand_grid(const SimulatorConfig& base, const std::vector<SweepAxis>& axes);

}
