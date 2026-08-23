# lob-simulator

A C++17 limit order book matching engine with price-time priority, plus a
replay/validation harness that runs it against real exchange data
([LOBSTER](https://lobsterdata.com/)) to check the matching logic against
what actually happened on the market.

No external dependencies, just CMake and a C++17 compiler.

## What's implemented

**Matching engine** (`include/order_book.hpp`, `src/order_book.cpp`)
- Price-time priority matching for `Limit`, `Market`, and `IOC` orders
- Order add / cancel / modify, with modify following real exchange semantics:
  shrinking an order's quantity keeps its place in the queue, growing it
  sends the order to the back and it loses time priority
- Best bid/ask, spread, depth at a price, and top-N book levels
- Bids and asks live in two `std::map`s with opposite comparators, so the
  best price on either side is always `.begin()`. One `match()` routine
  handles both buy and sell incoming orders through a generic walk over
  whichever side it's matching against, so there's no duplicated logic
  between the buy path and the sell path

**LOBSTER replay & validation** (`include/replayer.hpp`, `include/lobster_reader.hpp`,
`src/validate_replay.cpp`, `src/replay_cli.cpp`)
- Parses [LOBSTER](https://lobsterdata.com/) historical message files
  (real NASDAQ order-book data) and replays each event, submission,
  cancel, delete, execution, through the matching engine
- `lob_validate` bootstraps the book from a LOBSTER orderbook snapshot,
  replays the corresponding message file, and diffs the engine's book
  against LOBSTER's own snapshot level by level after every event, to
  check the matching engine against ground truth
- `lob_replay` runs a message file through the engine standalone and
  reports trade count, volume, VWAP, and final book state

**Synthetic order-flow simulator** (`include/simulator.hpp`, `src/simulator.cpp`,
`src/simulate_cli.cpp`)
- Generates random limit orders, market orders, and cancels against the
  matching engine using a zero-intelligence-style model: order arrival
  type is drawn from configurable relative rates, side is a coin flip,
  quantity is uniform, and limit price is offset from the opposite
  side's best quote by a geometric-distributed number of ticks (so most
  orders land near the touch, occasionally crossing and trading)
- Seeds an initial two-sided book before generating flow, so there's
  always something to trade against
- Cancels are sampled from actually-live resting order ids, tracked as
  orders rest and get consumed by later matches
- Fully deterministic given a seed (`std::mt19937_64`), for reproducible
  runs
- `lob_simulate` runs a configurable number of events and prints summary
  stats (event counts, trade count/volume/VWAP, final book state), with
  optional CSV output of the best-bid/best-ask time series and the trade
  tape

**Tests** (`tests/`), a self-contained `assert`-based suite with no gtest
dependency, covering matching, price-time priority, cancel/modify
semantics, the LOBSTER replay path, and simulator determinism/invariants,
run via CTest.

### Validated against real market data

`data/` holds LOBSTER's public sample files (AAPL, MSFT, INTC, GOOG,
AMZN, 2012-06-21). Running `lob_validate` against them confirms the
matching engine reproduces the real order book correctly: for AAPL,
level-1 (top-of-book) matches exactly for the first 440 real market
events before any divergence appears. That divergence is a known,
expected artifact of the sample data rather than a matching bug.
LOBSTER's snapshots only capture the top N price levels, so any liquidity
resting beyond that depth is invisible to the replay until a later
cancel or execution exposes it. Mismatch rates increase monotonically
with book depth across all five tickers tested, which is exactly the
signature you'd expect from that kind of information loss, not from a
broken matching rule.

## Build & test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
cd build && ctest --output-on-failure
```

## Run it

Sample data isn't redistributed in this repo, download the matching message/orderbook file pairs from LOBSTER into data/ before running the commands below.

```bash
# small hardcoded demo of the matching engine
./build/lob_sim

# replay a LOBSTER message file through the engine and print summary stats
./build/lob_replay data/AAPL_2012-06-21_34200000_57600000_message_10.csv

# replay + diff the engine's book against LOBSTER's own snapshot, level by level
./build/lob_validate \
  data/AAPL_2012-06-21_34200000_57600000_message_10.csv \
  data/AAPL_2012-06-21_34200000_57600000_orderbook_10.csv

# generate 20,000 synthetic events (seed 42) and print summary stats
./build/lob_simulate 20000 42

# same, plus CSV dumps of the best-bid/ask series and the trade tape
./build/lob_simulate 20000 42 --snapshots-csv snapshots.csv --trades-csv trades.csv
```

## What's not built yet