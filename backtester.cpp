#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <charconv>
#include <simdjson.h>
#include <memory_resource>
#include <array>

// --- 1. CORE LOGIC (Same as OrderBookEngine) ---
struct Level { double price; double quantity; };

double fast_atof(std::string_view str) {
    double result;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
    if (ec != std::errc()) return 0.0;
    return result;
}

class OrderBook {
private:
    std::pmr::vector<Level> bids; 
    std::pmr::vector<Level> asks;

public:
    OrderBook(std::pmr::memory_resource* pool) : bids(pool), asks(pool) {
        bids.reserve(5000); asks.reserve(5000);
    }
    void update_bid(double price, double qty) {
        auto it = std::lower_bound(bids.begin(), bids.end(), price, [](const Level& l, double val) { return l.price > val; });
        if (it != bids.end() && it->price == price) {
            if (qty <= 1e-7) bids.erase(it); else it->quantity = qty;
        } else if (qty > 1e-7) bids.insert(it, {price, qty});
    }
    void update_ask(double price, double qty) {
        auto it = std::lower_bound(asks.begin(), asks.end(), price, [](const Level& l, double val) { return l.price < val; });
        if (it != asks.end() && it->price == price) {
            if (qty <= 1e-7) asks.erase(it); else it->quantity = qty;
        } else if (qty > 1e-7) asks.insert(it, {price, qty});
    }
    double get_imbalance() {
        if (bids.empty() || asks.empty()) return 0.5;
        double b=0, a=0;
        for(size_t i=0; i<std::min((size_t)5, bids.size()); ++i) b+=bids[i].quantity;
        for(size_t i=0; i<std::min((size_t)5, asks.size()); ++i) a+=asks[i].quantity;
        return b/(b+a);
    }
    double get_best_bid() { return bids.empty() ? 0.0 : bids[0].price; }
    double get_best_ask() { return asks.empty() ? 0.0 : asks[0].price; }
};

// --- 2. VIRTUAL WALLET ---
class BacktestWallet {
public:
    double usd_balance = 10000.0; 
    double btc_balance = 0.0;
    int trade_count = 0;

    void execute(const std::string& side, double price, double quantity) {
        if (side == "BUY") {
            usd_balance -= (price * quantity);
            btc_balance += quantity;
        } else {
            usd_balance += (price * quantity);
            btc_balance -= quantity;
        }
        trade_count++;
    }

    double get_total_equity(double current_price) {
        return usd_balance + (btc_balance * current_price);
    }
};

// --- 3. MAIN SIMULATION ---
int main() {
    alignas(std::max_align_t) std::array<std::byte, 1024*1024> buf;
    std::pmr::monotonic_buffer_resource pool{buf.data(), buf.size()};
    OrderBook book(&pool);
    BacktestWallet wallet;

    std::ifstream log_file("market_data.log");
    if (!log_file.is_open()) {
        std::cerr << "Error: market_data.log not found inside build folder!" << std::endl;
        return 1;
    }

    std::cout << "[BACKTEST] Starting simulation..." << std::endl;
    std::string line;
    simdjson::dom::parser parser;
    int cooldown = 0;
    int processed = 0;

    // --- REPLAY LOOP ---
    while (std::getline(log_file, line)) {
        processed++;
        if (line.empty()) continue;

        try {
            simdjson::dom::element doc = parser.parse(line);
            simdjson::dom::array bids = doc["b"];
            simdjson::dom::array asks = doc["a"];

            for (auto l : bids) book.update_bid(fast_atof(l.at(0)), fast_atof(l.at(1)));
            for (auto l : asks) book.update_ask(fast_atof(l.at(0)), fast_atof(l.at(1)));

            if (cooldown > 0) cooldown--;
            if (cooldown == 0 && book.get_best_ask() > book.get_best_bid()) {
                double imb = book.get_imbalance();
                double trade_qty = 0.002;

                if (imb > 0.8) { 
                    wallet.execute("BUY", book.get_best_ask(), trade_qty);
                    cooldown = 100; 
                } 
                else if (imb < 0.2) { 
                    wallet.execute("SELL", book.get_best_bid(), trade_qty);
                    cooldown = 100;
                }
            }
        } catch (const simdjson::simdjson_error& e) {
            std::cerr << "[WARNING] Skipping bad line #" << processed << std::endl;
            continue;
        }
    }

    // --- FINAL REPORT ---
    double final_price = (book.get_best_bid() + book.get_best_ask()) / 2.0;
    if (final_price == 0) final_price = 90000.0; // Fallback if book is empty

    double start_equity = 10000.0;
    double end_equity = wallet.get_total_equity(final_price);
    
    std::cout << "\n=== BACKTEST RESULTS ===" << std::endl;
    std::cout << "Updates Processed: " << processed << std::endl;
    std::cout << "Trades Executed:   " << wallet.trade_count << std::endl;
    std::cout << "Starting Equity:   $" << start_equity << std::endl;
    std::cout << "Final Equity:      $" << end_equity << std::endl;
    std::cout << "Net PnL:           $" << (end_equity - start_equity) << std::endl;
    std::cout << "========================" << std::endl;

    return 0;
}