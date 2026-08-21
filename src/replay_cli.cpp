#include "replayer.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <lobster_message_file>\n";
        return 1;
    }

    try {
        std::string path(argv[1]);
        lob::Replayer replayer(path);

        std::vector<lob::Trade> trades;
        std::size_t event_count = 0;
        std::size_t trade_count = 0;
        lob::Quantity traded_qty = 0;
        double notional = 0.0;

        while (replayer.step(trades)) {
            ++event_count;
            for (const auto& t : trades) {
                ++trade_count;
                traded_qty += t.qty;
                notional += static_cast<double>(t.price) * static_cast<double>(t.qty);
            }
            trades.clear();
        }

        std::cout << "events processed: " << event_count << "\n";
        std::cout << "trades: " << trade_count << "\n";
        std::cout << "total traded qty: " << traded_qty << "\n";
        if (traded_qty > 0) {
            std::cout << "vwap: " << (notional / static_cast<double>(traded_qty)) << "\n";
        }

        const auto& book = replayer.book();
        if (auto bid = book.best_bid()) {
            std::cout << "final best bid: " << *bid << "\n";
        }
        if (auto ask = book.best_ask()) {
            std::cout << "final best ask: " << *ask << "\n";
        }
        if (auto spread = book.spread()) {
            std::cout << "final spread: " << *spread << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
