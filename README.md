# lob-simulator

A C++17 limit order book matching engine with price-time priority, plus a
replay harness that runs it against real exchange data
([LOBSTER](https://lobsterdata.com/)) to check the matching logic against
what actually happened on the market.

No external dependencies — just CMake and a C++17 compiler.

## What's implemented

The matching engine handles `Limit`, `Market`, and `IOC` orders with
price-time priority, plus add/cancel/modify. Modify follows real exchange
semantics: shrinking an order keeps its place in the queue, growing it sends
the order to the back. Bids and asks are stored in `std::map`s with opposite
comparators so `.begin()` is always the best price on each side, and one
generic `match()` routine handles both buy and sell orders instead of
duplicating the logic for each direction.

On top of that there's a LOBSTER replay and validation harness. It parses
LOBSTER message files and replays each event — submission, cancel, delete,
execution — through the engine. `lob_validate` bootstraps the book from a
LOBSTER orderbook snapshot and then diffs the engine's book against
LOBSTER's own snapshot level-by-level after every event, so you can see
where it drifts. `lob_replay` just runs a message file
standalone and prints trade count, volume, VWAP, and final book state.

Tests are plain `assert`-based (no gtest), run through CTest.

### Checked against real market data

`data/` has LOBSTER's public sample files for AAPL, MSFT, INTC, GOOG, and
AMZN from 2012-06-21. Running `lob_validate` against them shows the engine
reproducing the real book correctly — for AAPL, top-of-book matches for the
first 440 events. The divergence after that isn't a matching bug — LOBSTER's
snapshots only capture the top N levels, so any liquidity resting below that
depth is invisible to the replay until a later cancel or execution exposes
it. Mismatch rates climb with book depth across all five tickers, which
lines up with that explanation.

## Build & test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
cd build && ctest --output-on-failure
```

## Run it

```bash
# small hardcoded demo of the matching engine
./build/lob_sim

# replay a LOBSTER message file and print summary stats
./build/lob_replay data/AAPL_2012-06-21_34200000_57600000_message_10.csv

# replay + diff against LOBSTER's own snapshot, level by level
./build/lob_validate \
  data/AAPL_2012-06-21_34200000_57600000_message_10.csv \
  data/AAPL_2012-06-21_34200000_57600000_orderbook_10.csv
```

## Not built yet

This is a matching engine and a validation harness, not a full simulator.
There's no order-flow generator, agent-based traders, or experiment harness
on top of it — that's the next piece.
