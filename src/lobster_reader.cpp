#include "lobster_reader.hpp"

#include <istream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace lob {

namespace {

LobsterMsgType parse_type(int raw) {
    switch (raw) {
        case 1: return LobsterMsgType::Submission;
        case 2: return LobsterMsgType::PartialCancel;
        case 3: return LobsterMsgType::Deletion;
        case 4: return LobsterMsgType::ExecutionVisible;
        case 5: return LobsterMsgType::ExecutionHidden;
        case 6: return LobsterMsgType::Cross;
        case 7: return LobsterMsgType::Halt;
        default:
            throw std::runtime_error("lobster_reader: unknown message type " + std::to_string(raw));
    }
}

}  // namespace

LobsterReader::LobsterReader(std::istream& in) : in_(in) {}

bool LobsterReader::next(LobsterEvent& out) {
    std::string line;
    while (std::getline(in_, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream ss(line);
        std::string field;
        LobsterEvent parsed;
        int type_raw;

        try {
            std::getline(ss, field, ','); parsed.time_sec = std::stod(field);
            std::getline(ss, field, ','); type_raw = std::stoi(field);
            std::getline(ss, field, ','); parsed.order_id = std::stoll(field);
            std::getline(ss, field, ','); parsed.size = std::stoll(field);
            std::getline(ss, field, ','); parsed.price = std::stoll(field);
            std::getline(ss, field, ','); parsed.direction = std::stoi(field);
        } catch (const std::exception&) {
            throw std::runtime_error("lobster_reader: malformed row: " + line);
        }

        parsed.type = parse_type(type_raw);
        out = parsed;
        return true;
    }
    return false;
}

}  // namespace lob
