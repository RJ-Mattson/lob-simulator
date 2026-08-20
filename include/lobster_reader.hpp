#pragma once

#include "order.hpp"

#include <iosfwd>

namespace lob {

// Message types from a LOBSTER (https://lobsterdata.com) message file.
// Types 6 (cross trade) and 7 (trading halt) are recognized but not acted
// on by Replayer -- see replayer.hpp.
enum class LobsterMsgType {
    Submission = 1,
    PartialCancel = 2,
    Deletion = 3,
    ExecutionVisible = 4,
    ExecutionHidden = 5,
    Cross = 6,
    Halt = 7,
};

// One parsed row of a LOBSTER message file:
// Time,Type,OrderID,Size,Price,Direction
struct LobsterEvent {
    double time_sec = 0.0;  // seconds since midnight
    LobsterMsgType type = LobsterMsgType::Submission;
    OrderId order_id = 0;
    Quantity size = 0;
    Price price = 0;
    int direction = 0;  // 1 = buy side, -1 = sell side
};

// Streams rows out of a LOBSTER message file (CSV, no header), one at a time.
class LobsterReader {
public:
    // `in` must outlive this reader.
    explicit LobsterReader(std::istream& in);

    // Parses the next row into `out`. Returns false at end of stream.
    // Throws std::runtime_error on a malformed row.
    bool next(LobsterEvent& out);

private:
    std::istream& in_;
};

}  // namespace lob
