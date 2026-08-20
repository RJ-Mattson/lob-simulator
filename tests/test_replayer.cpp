#include "lobster_reader.hpp"
#include "replayer.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << "\n";
        ++failures;
    }
}

void test_lobster_reader_parses_rows() {
    std::istringstream in(
        "34200.189329536,1,100,10,100,1\n"
        "34201.5,4,101,5,102,-1\n");
    lob::LobsterReader reader(in);

    lob::LobsterEvent event;
    check(reader.next(event), "should read first row");
    check(event.type == lob::LobsterMsgType::Submission, "first row should be a submission");
    check(event.order_id == 100, "first row order id should be 100");
    check(event.size == 10, "first row size should be 10");
    check(event.price == 100, "first row price should be 100");
    check(event.direction == 1, "first row direction should be 1");

    check(reader.next(event), "should read second row");
    check(event.type == lob::LobsterMsgType::ExecutionVisible, "second row should be an execution");
    check(event.order_id == 101, "second row order id should be 101");

    check(!reader.next(event), "should be no third row");
}

void test_lobster_reader_rejects_malformed_row() {
    std::istringstream in("not,a,valid,row\n");
    lob::LobsterReader reader(in);
    lob::LobsterEvent event;
    bool threw = false;
    try {
        reader.next(event);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "malformed row should throw");
}

void test_replayer_full_lifecycle() {
    std::istringstream in(
        "1.0,1,100,10,100,1\n"    // submit buy id100 qty10 @100
        "2.0,1,101,5,102,-1\n"    // submit sell id101 qty5 @102
        "3.0,2,100,4,100,1\n"     // partial cancel: id100 10 -> 6
        "4.0,4,101,5,102,-1\n"    // execution: consumes all of id101
        "5.0,5,999,2,102,-1\n"    // hidden execution, unknown id -> no trade
        "6.0,3,100,6,100,1\n");   // full deletion of id100
    lob::Replayer replayer(in);

    std::vector<lob::Trade> trades;

    replayer.step(trades);
    check(replayer.book().depth_at(lob::OrderSide::Buy, 100) == 10, "id100 should rest with qty10");

    replayer.step(trades);
    check(replayer.book().depth_at(lob::OrderSide::Sell, 102) == 5, "id101 should rest with qty5");

    replayer.step(trades);
    check(replayer.book().depth_at(lob::OrderSide::Buy, 100) == 6, "partial cancel should shrink id100 to 6");

    trades.clear();
    replayer.step(trades);
    check(trades.size() == 1, "execution should produce one trade");
    if (!trades.empty()) {
        check(trades[0].sellId == 101, "trade should be against the resting sell order");
        check(trades[0].price == 102, "trade price should be the resting order's price");
        check(trades[0].qty == 5, "trade qty should be 5");
    }
    check(replayer.book().depth_at(lob::OrderSide::Sell, 102) == 0, "id101 should be fully consumed");

    trades.clear();
    replayer.step(trades);
    check(trades.empty(), "hidden-order execution should produce no trade since hidden liquidity isn't modeled");

    replayer.step(trades);
    check(replayer.book().depth_at(lob::OrderSide::Buy, 100) == 0, "id100 should be fully deleted");
    check(replayer.book().best_bid() == std::nullopt, "book should have no bids left");
    check(replayer.book().best_ask() == std::nullopt, "book should have no asks left");
}

}  // namespace

int main() {
    test_lobster_reader_parses_rows();
    test_lobster_reader_rejects_malformed_row();
    test_replayer_full_lifecycle();

    if (failures > 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
