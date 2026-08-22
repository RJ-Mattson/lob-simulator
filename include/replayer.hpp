#pragma once

#include "lobster_reader.hpp"
#include "order_book.hpp"

#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lob {

class Replayer {
public:
    explicit Replayer(const std::string& message_file_path);
    explicit Replayer(std::istream& message_stream);

    bool step(std::vector<Trade>& trades_out);

    void bootstrap(const std::vector<std::pair<Price, Quantity>>& bid_levels,
                    const std::vector<std::pair<Price, Quantity>>& ask_levels);

    OrderBook& book() { return book_; }
    const OrderBook& book() const { return book_; }
    const LobsterEvent& last_event() const { return last_event_; }

private:
    std::ifstream owned_file_;
    LobsterReader reader_;
    OrderBook book_;
    std::unordered_map<OrderId, Quantity> resting_qty_;
    LobsterEvent last_event_{};
    OrderId next_synthetic_id_ = -1;

    void apply(const LobsterEvent& event, std::vector<Trade>& trades_out);
};

}
