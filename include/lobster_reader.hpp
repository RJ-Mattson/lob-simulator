#pragma once

#include "order.hpp"

#include <iosfwd>
#include <string>

namespace lob {

enum class LobsterMsgType {
    Submission = 1,
    PartialCancel = 2,
    Deletion = 3,
    ExecutionVisible = 4,
    ExecutionHidden = 5,
    Cross = 6,
    Halt = 7,
};

struct LobsterEvent {
    double time_sec = 0.0;
    LobsterMsgType type = LobsterMsgType::Submission;
    OrderId order_id = 0;
    Quantity size = 0;
    Price price = 0;
    int direction = 0;
};

class LobsterReader {
public:
    explicit LobsterReader(std::istream& in);

    bool next(LobsterEvent& out);

private:
    std::istream& in_;
    std::string line_;
};

}
