#include "replayer.hpp"

#include <charconv>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::vector<long long> parse_row(const std::string& line) {
    std::vector<long long> fields;
    std::size_t pos = 0;
    while (pos <= line.size()) {
        std::size_t comma = line.find(',', pos);
        std::size_t end = (comma == std::string::npos) ? line.size() : comma;
        std::string_view field(line.data() + pos, end - pos);

        long long value = 0;
        std::from_chars(field.data(), field.data() + field.size(), value);
        fields.push_back(value);

        if (comma == std::string::npos) {
            break;
        }
        pos = comma + 1;
    }
    return fields;
}

}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <lobster_message_file> <lobster_orderbook_file>\n";
        return 1;
    }

    std::string message_path(argv[1]);
    std::string orderbook_path(argv[2]);

    std::ifstream orderbook_file(orderbook_path);
    if (!orderbook_file.is_open()) {
        std::cerr << "error: could not open orderbook file: " << orderbook_path << "\n";
        return 1;
    }

    std::ifstream message_file(message_path);
    if (!message_file.is_open()) {
        std::cerr << "error: could not open message file: " << message_path << "\n";
        return 1;
    }

    constexpr std::size_t max_reported = 20;
    constexpr long long ask_sentinel = 9999999999LL;
    constexpr long long bid_sentinel = -9999999999LL;

    std::string bootstrap_line;
    do {
        if (!std::getline(orderbook_file, bootstrap_line)) {
            std::cerr << "error: orderbook file has no rows\n";
            return 1;
        }
    } while (bootstrap_line.empty());

    std::string first_message_line;
    do {
        if (!std::getline(message_file, first_message_line)) {
            std::cerr << "error: message file has no rows\n";
            return 1;
        }
    } while (first_message_line.empty());

    std::vector<long long> bootstrap_fields = parse_row(bootstrap_line);
    if (bootstrap_fields.empty() || bootstrap_fields.size() % 4 != 0) {
        std::cerr << "error: malformed orderbook row 1\n";
        return 1;
    }
    std::size_t bootstrap_levels = bootstrap_fields.size() / 4;

    std::vector<std::pair<lob::Price, lob::Quantity>> bid_levels;
    std::vector<std::pair<lob::Price, lob::Quantity>> ask_levels;
    for (std::size_t lvl = 0; lvl < bootstrap_levels; ++lvl) {
        ask_levels.emplace_back(bootstrap_fields[lvl * 4 + 0], bootstrap_fields[lvl * 4 + 1]);
        bid_levels.emplace_back(bootstrap_fields[lvl * 4 + 2], bootstrap_fields[lvl * 4 + 3]);
    }

    try {
        lob::Replayer replayer(message_file);
        replayer.bootstrap(bid_levels, ask_levels);

        std::vector<lob::Trade> trades;
        std::string line;
        std::size_t line_no = 1;
        std::size_t mismatches = 0;
        std::size_t bid_mismatches = 0;
        std::size_t ask_mismatches = 0;
        std::size_t last_bid_mismatch_line = 0;
        std::size_t last_ask_mismatch_line = 0;
        std::size_t first_bid_mismatch_line = 0;
        std::size_t first_ask_mismatch_line = 0;
        std::vector<std::size_t> mismatches_per_level(bootstrap_levels, 0);

        while (replayer.step(trades)) {
            ++line_no;
            trades.clear();

            if (!std::getline(orderbook_file, line)) {
                std::cerr << "error: orderbook file ended early at line " << line_no << "\n";
                return 1;
            }

            std::vector<long long> fields = parse_row(line);
            if (fields.empty() || fields.size() % 4 != 0) {
                std::cerr << "error: malformed orderbook row " << line_no << "\n";
                return 1;
            }
            std::size_t levels = fields.size() / 4;

            auto asks = replayer.book().top_levels(lob::OrderSide::Sell, levels);
            auto bids = replayer.book().top_levels(lob::OrderSide::Buy, levels);

            for (std::size_t lvl = 0; lvl < levels; ++lvl) {
                long long expected_ask_price = fields[lvl * 4 + 0];
                long long expected_ask_size = fields[lvl * 4 + 1];
                long long expected_bid_price = fields[lvl * 4 + 2];
                long long expected_bid_size = fields[lvl * 4 + 3];

                long long actual_ask_price = (lvl < asks.size()) ? asks[lvl].first : ask_sentinel;
                long long actual_ask_size = (lvl < asks.size()) ? asks[lvl].second : 0;
                long long actual_bid_price = (lvl < bids.size()) ? bids[lvl].first : bid_sentinel;
                long long actual_bid_size = (lvl < bids.size()) ? bids[lvl].second : 0;

                bool ask_ok = actual_ask_price == expected_ask_price && actual_ask_size == expected_ask_size;
                bool bid_ok = actual_bid_price == expected_bid_price && actual_bid_size == expected_bid_size;

                if (!ask_ok) {
                    ++ask_mismatches;
                    last_ask_mismatch_line = line_no;
                    if (first_ask_mismatch_line == 0) {
                        first_ask_mismatch_line = line_no;
                    }
                }
                if (!bid_ok) {
                    ++bid_mismatches;
                    last_bid_mismatch_line = line_no;
                    if (first_bid_mismatch_line == 0) {
                        first_bid_mismatch_line = line_no;
                    }
                }

                if (!ask_ok || !bid_ok) {
                    ++mismatches;
                    if (lvl < mismatches_per_level.size()) {
                        ++mismatches_per_level[lvl];
                    }
                    if (mismatches <= max_reported) {
                        std::cerr << "mismatch at message line " << line_no << " level " << (lvl + 1) << ":\n"
                                  << "  ask expected=(" << expected_ask_price << "," << expected_ask_size
                                  << ") actual=(" << actual_ask_price << "," << actual_ask_size << ")\n"
                                  << "  bid expected=(" << expected_bid_price << "," << expected_bid_size
                                  << ") actual=(" << actual_bid_price << "," << actual_bid_size << ")\n";
                    }
                }
            }
        }

        std::cout << "bootstrapped from orderbook row 1, compared " << (line_no - 1) << " events\n";
        std::cout << "mismatches: " << mismatches << "\n";
        std::cout << "bid mismatches: " << bid_mismatches << " (first at line " << first_bid_mismatch_line
                   << ", last at line " << last_bid_mismatch_line << ")\n";
        std::cout << "ask mismatches: " << ask_mismatches << " (first at line " << first_ask_mismatch_line
                   << ", last at line " << last_ask_mismatch_line << ")\n";
        for (std::size_t lvl = 0; lvl < mismatches_per_level.size(); ++lvl) {
            std::cout << "  level " << (lvl + 1) << " mismatches: " << mismatches_per_level[lvl] << " / "
                      << (line_no - 1) << "\n";
        }
        return mismatches == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
