#pragma once

#include "lobster_reader.hpp"
#include "order_book.hpp"

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace lob {

// Replays a LOBSTER message file through a real OrderBook, so that trades
// emerge from our own matching logic rather than being copied off the tape.
//
// LOBSTER only logs the *resting* side of a trade (a submission row for
// the passive order, later an execution row against its id) -- the
// marketable order that triggers the execution is never itself logged. To
// replay through match() we synthesize that missing aggressor: an IOC
// order, opposite side, with a synthetic negative id (real LOBSTER ids are
// always positive, so these can't collide), fed into add_order() so the
// engine's own matching consumes the resting order.
//
// Hidden-order executions (type 5) reference an order id that was never
// submitted to this book (LOBSTER doesn't log hidden liquidity), so the
// synthesized aggressor naturally finds nothing to match -- this book does
// not model hidden liquidity, by construction.
class Replayer {
public:
    explicit Replayer(const std::string& message_file_path);
    // `message_stream` must outlive this Replayer.
    explicit Replayer(std::istream& message_stream);

    // Processes the next event from the message file, applying it to the
    // book. Appends any resulting trades to `trades_out` and returns true.
    // Returns false once the file is exhausted.
    bool step(std::vector<Trade>& trades_out);

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

}  // namespace lob
