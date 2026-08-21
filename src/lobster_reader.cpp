#include "lobster_reader.hpp"

#include <charconv>
#include <istream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace lob {

namespace {

LobsterMsgType parse_type(int raw, const std::string& line) {
    switch (raw) {
        case 1: return LobsterMsgType::Submission;
        case 2: return LobsterMsgType::PartialCancel;
        case 3: return LobsterMsgType::Deletion;
        case 4: return LobsterMsgType::ExecutionVisible;
        case 5: return LobsterMsgType::ExecutionHidden;
        case 6: return LobsterMsgType::Cross;
        case 7: return LobsterMsgType::Halt;
        default:
            throw std::runtime_error("lobster_reader: unknown message type " + std::to_string(raw) +
                                      " in row: " + line);
    }
}

std::string_view next_field(const std::string& line, std::size_t& pos) {
    std::size_t comma = line.find(',', pos);
    std::size_t end = (comma == std::string::npos) ? line.size() : comma;
    std::string_view field(line.data() + pos, end - pos);
    pos = (comma == std::string::npos) ? line.size() : comma + 1;
    return field;
}

template <typename T>
T parse_number(std::string_view field, const std::string& line) {
    T value{};
    auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), value);
    if (ec != std::errc() || ptr != field.data() + field.size()) {
        throw std::runtime_error("lobster_reader: malformed row: " + line);
    }
    return value;
}

}

LobsterReader::LobsterReader(std::istream& in) : in_(in) {}

bool LobsterReader::next(LobsterEvent& out) {
    while (std::getline(in_, line_)) {
        if (line_.empty()) {
            continue;
        }

        std::size_t pos = 0;
        LobsterEvent parsed;
        parsed.time_sec = parse_number<double>(next_field(line_, pos), line_);
        int type_raw = parse_number<int>(next_field(line_, pos), line_);
        parsed.order_id = parse_number<OrderId>(next_field(line_, pos), line_);
        parsed.size = parse_number<Quantity>(next_field(line_, pos), line_);
        parsed.price = parse_number<Price>(next_field(line_, pos), line_);
        parsed.direction = parse_number<int>(next_field(line_, pos), line_);
        parsed.type = parse_type(type_raw, line_);

        out = parsed;
        return true;
    }
    return false;
}

}
