#include "replayer.hpp"

#include <iterator>
#include <stdexcept>

namespace lob {

Replayer::Replayer(const std::string& message_file_path)
    : owned_file_(message_file_path), reader_(owned_file_) {
    if (!owned_file_.is_open()) {
        throw std::runtime_error("Replayer: could not open message file: " + message_file_path);
    }
}

Replayer::Replayer(std::istream& message_stream) : reader_(message_stream) {}

bool Replayer::step(std::vector<Trade>& trades_out) {
    LobsterEvent event;
    if (!reader_.next(event)) {
        return false;
    }
    last_event_ = event;
    apply(event, trades_out);
    return true;
}

void Replayer::apply(const LobsterEvent& event, std::vector<Trade>& trades_out) {
    switch (event.type) {
        case LobsterMsgType::Submission: {
            OrderSide side = (event.direction == 1) ? OrderSide::Buy : OrderSide::Sell;
            Order order(event.order_id, side, OrderType::Limit, event.price, event.size, book_.next_timestamp());
            auto trades = book_.add_order(order);
            trades_out.insert(trades_out.end(), std::make_move_iterator(trades.begin()),
                               std::make_move_iterator(trades.end()));
            resting_qty_[event.order_id] = event.size;
            break;
        }
        case LobsterMsgType::PartialCancel: {
            auto it = resting_qty_.find(event.order_id);
            if (it != resting_qty_.end()) {
                Quantity new_qty = it->second - event.size;
                book_.modify_order(event.order_id, new_qty);
                if (new_qty > 0) {
                    it->second = new_qty;
                } else {
                    resting_qty_.erase(it);
                }
            }
            break;
        }
        case LobsterMsgType::Deletion: {
            book_.cancel_order(event.order_id);
            resting_qty_.erase(event.order_id);
            break;
        }
        case LobsterMsgType::ExecutionVisible: {
            OrderSide resting_side = (event.direction == 1) ? OrderSide::Buy : OrderSide::Sell;
            OrderSide aggressor_side = (resting_side == OrderSide::Buy) ? OrderSide::Sell : OrderSide::Buy;
            Order aggressor(next_synthetic_id_--, aggressor_side, OrderType::IOC, event.price, event.size,
                             book_.next_timestamp());
            auto trades = book_.add_order(aggressor);
            trades_out.insert(trades_out.end(), std::make_move_iterator(trades.begin()),
                               std::make_move_iterator(trades.end()));

            auto it = resting_qty_.find(event.order_id);
            if (it != resting_qty_.end()) {
                it->second -= event.size;
                if (it->second <= 0) {
                    resting_qty_.erase(it);
                }
            }
            break;
        }
        case LobsterMsgType::ExecutionHidden:
        case LobsterMsgType::Cross:
        case LobsterMsgType::Halt:
            break;
    }
}

}
