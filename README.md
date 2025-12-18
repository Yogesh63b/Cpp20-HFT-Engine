# ⚡ Velocity: High-Frequency Trading (HFT) Engine

A low-latency, zero-allocation event-driven trading engine built in C++20. Designed for crypto market making and arbitrage strategies with nanosecond-precision execution paths.

## 🚀 Key Features

* **Zero-Allocation Architecture:** Uses `std::pmr` (Polymorphic Memory Resources) and Stack Arenas to eliminate heap allocations (`malloc`/`free`) on the hot path, ensuring deterministic latency.
* **Hybrid Connectivity:** Initializes state via HTTP REST Snapshots and maintains real-time state via SSL WebSockets (Boost.Beast).
* **Vectorized Parsing:** Utilizes `simdjson` for AVX2/NEON-accelerated JSON parsing (2-3x faster than standard parsers).
* **Contiguous Memory Order Book:** Custom `std::pmr::vector` based LOB (Limit Order Book) optimized for CPU L1/L2 cache locality.
* **Risk Gateway:** Integrated pre-trade risk checks for Max Position, Fat Finger protection, and Wash Trade prevention.
* **Backtesting Engine:** Includes a robust event-replay simulator to validate strategies against recorded market data tick-by-tick.

## 🏗️ System Architecture


graph TD
    A[Exchange / WebSocket] -->|JSON Stream| B(Network Handler)
    B -->|Raw Bytes| C{SIMD Parser}
    C -->|Price/Size| D[Order Book Engine]
    D -->|Imbalance Signal| E[Strategy Logic]
    E -->|Order Request| F{Risk Manager}
    F -->|Approved| G[Execution Gateway]
    F -->|Rejected| H[Log & Drop]
    G -->|FIX/HTTP| I[Exchange Matching Engine]

    subgraph "Zero-Allocation Zone"
    C
    D
    E
    F
    G
    end

    🛠️ Technology Stack
Language: C++20

Networking: Boost.Asio, Boost.Beast, OpenSSL

Parsing: simdjson

Memory Management: std::pmr::monotonic_buffer_resource

Build System: CMake

📊 Performance Benchmarks
Environment: Apple M-Series (ARM64) / Linux x86_64

Wire-to-Order Latency: ~21 microseconds (avg)

Parsing Speed: < 2 microseconds per packet

Jitter: Eliminated via Stack Arena allocation strategy.

📂 Project Structure
Bash

├── CMakeLists.txt       # Build configuration
├── orderbook.cpp        # Main HFT Engine (Live Trading)
├── backtester.cpp       # Replay Engine (Strategy Testing)
└── README.md            # Documentation
⚙️ Build & Run
Prerequisites
CMake (3.15+)

C++20 Compiler (Clang/GCC)

Boost Libraries

OpenSSL

simdjson

Compilation
Bash

mkdir build && cd build
cmake ..
make
Running the Engine
Bash

./OrderBookEngine
Connects to Binance.US WebSocket feed, synchronizes order book, and begins trading logic.

Running the Backtester
Record data by running the Engine for a few minutes (logs to market_data.log).

Run the replay:

Bash

./Backtester
🛡️ Risk Management
The system enforces strict pre-trade limits:

Max Notional: Orders > $2,000 are rejected.

Position Limit: Net inventory > 0.01 BTC is rejected.

Cooldowns: Prevents strategy spamming during high volatility.

📈 Future Roadmap
[ ] Implement Lock-Free Ring Buffer (LMAX Disruptor) for thread separation.

[ ] Add FIX Protocol (4.2/4.4) for institutional connectivity.

[ ] Kernel Bypass (Solarflare/DPDK) for sub-microsecond networking.

Disclaimer: This software is for educational purposes. Use at your own risk.


### **How to do it**
1.  Open VS Code (or your text editor).# ⚡ Velocity: High-Frequency Trading (HFT) Engine

A low-latency, zero-allocation event-driven trading engine built in C++20. Designed for crypto market making and arbitrage strategies with nanosecond-precision execution paths.

## 🚀 Key Features

* **Zero-Allocation Architecture:** Uses `std::pmr` (Polymorphic Memory Resources) and Stack Arenas to eliminate heap allocations (`malloc`/`free`) on the hot path, ensuring deterministic latency.
* **Hybrid Connectivity:** Initializes state via HTTP REST Snapshots and maintains real-time state via SSL WebSockets (Boost.Beast).
* **Vectorized Parsing:** Utilizes `simdjson` for AVX2/NEON-accelerated JSON parsing (2-3x faster than standard parsers).
* **Contiguous Memory Order Book:** Custom `std::pmr::vector` based LOB (Limit Order Book) optimized for CPU L1/L2 cache locality.
* **Risk Gateway:** Integrated pre-trade risk checks for Max Position, Fat Finger protection, and Wash Trade prevention.
* **Backtesting Engine:** Includes a robust event-replay simulator to validate strategies against recorded market data tick-by-tick.

## 🏗️ System Architecture


graph TD
    A[Exchange / WebSocket] -->|JSON Stream| B(Network Handler)
    B -->|Raw Bytes| C{SIMD Parser}
    C -->|Price/Size| D[Order Book Engine]
    D -->|Imbalance Signal| E[Strategy Logic]
    E -->|Order Request| F{Risk Manager}
    F -->|Approved| G[Execution Gateway]
    F -->|Rejected| H[Log & Drop]
    G -->|FIX/HTTP| I[Exchange Matching Engine]

    subgraph "Zero-Allocation Zone"
    C
    D
    E
    F
    G
    end
🛠️ Technology Stack
Language: C++20

Networking: Boost.Asio, Boost.Beast, OpenSSL

Parsing: simdjson

Memory Management: std::pmr::monotonic_buffer_resource

Build System: CMake

📊 Performance Benchmarks
Environment: Apple M-Series (ARM64) / Linux x86_64

Wire-to-Order Latency: ~21 microseconds (avg)

Parsing Speed: < 2 microseconds per packet

Jitter: Eliminated via Stack Arena allocation strategy.

📂 Project Structure
Bash

├── CMakeLists.txt       # Build configuration
├── orderbook.cpp        # Main HFT Engine (Live Trading)
├── backtester.cpp       # Replay Engine (Strategy Testing)
└── README.md            # Documentation
⚙️ Build & Run
Prerequisites
CMake (3.15+)

C++20 Compiler (Clang/GCC)

Boost Libraries

OpenSSL

simdjson

Compilation
Bash

mkdir build && cd build
cmake ..
make
Running the Engine
Bash

./OrderBookEngine
Connects to Binance.US WebSocket feed, synchronizes order book, and begins trading logic.

Running the Backtester
Record data by running the Engine for a few minutes (logs to market_data.log).

Run the replay:

Bash

./Backtester
🛡️ Risk Management
The system enforces strict pre-trade limits:

Max Notional: Orders > $2,000 are rejected.

Position Limit: Net inventory > 0.01 BTC is rejected.

Cooldowns: Prevents strategy spamming during high volatility.

📈 Future Roadmap

[ ] Implement Lock-Free Ring Buffer (LMAX Disruptor) for thread separation.

[ ] Add FIX Protocol (4.2/4.4) for institutional connectivity.

[ ] Kernel Bypass (Solarflare/DPDK) for sub-microsecond networking.

Disclaimer: This software is for educational purposes only.
