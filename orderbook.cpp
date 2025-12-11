#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <iostream>
#include <fstream> // <--- REQUIRED FOR LOGGING
#include <string>
#include <vector>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <simdjson.h>
#include <memory_resource>
#include <array>
#include <cmath>

namespace beast = boost::beast;         
namespace http = beast::http;           
namespace websocket = beast::websocket; 
namespace net = boost::asio;            
namespace ssl = boost::asio::ssl;       
using tcp = boost::asio::ip::tcp;       

// --- 1. DATA STRUCTURES ---
struct Level {
    double price;
    double quantity;
};

// --- 2. HELPER: Fast String Parsing ---
double fast_atof(std::string_view str) {
    double result;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
    if (ec != std::errc()) return 0.0;
    return result;
}

// --- 3. MEMORY OPTIMIZED ORDER BOOK ---
class OrderBook {
private:
    std::pmr::vector<Level> bids; 
    std::pmr::vector<Level> asks;

public:
    OrderBook(std::pmr::memory_resource* pool) 
        : bids(pool), asks(pool) {
        bids.reserve(5000); 
        asks.reserve(5000);
    }

    void update_bid(double price, double qty) {
        auto it = std::lower_bound(bids.begin(), bids.end(), price, 
            [](const Level& l, double val) { return l.price > val; });

        if (it != bids.end() && it->price == price) {
            if (qty <= 0.0000001) bids.erase(it);
            else it->quantity = qty;
        } else if (qty > 0.0000001) {
            bids.insert(it, {price, qty});
        }
    }

    void update_ask(double price, double qty) {
        auto it = std::lower_bound(asks.begin(), asks.end(), price, 
            [](const Level& l, double val) { return l.price < val; });

        if (it != asks.end() && it->price == price) {
            if (qty <= 0.0000001) asks.erase(it);
            else it->quantity = qty;
        } else if (qty > 0.0000001) {
            asks.insert(it, {price, qty});
        }
    }

    void load_snapshot(simdjson::dom::array& bid_array, simdjson::dom::array& ask_array) {
        bids.clear();
        asks.clear();
        std::cout << "[SNAPSHOT] Loading " << bid_array.size() << " bids and " << ask_array.size() << " asks..." << std::endl;
        for (simdjson::dom::array level : bid_array) bids.push_back({ fast_atof(level.at(0)), fast_atof(level.at(1)) });
        for (simdjson::dom::array level : ask_array) asks.push_back({ fast_atof(level.at(0)), fast_atof(level.at(1)) });
        std::sort(bids.begin(), bids.end(), [](const Level& a, const Level& b) { return a.price > b.price; });
        std::sort(asks.begin(), asks.end(), [](const Level& a, const Level& b) { return a.price < b.price; });
    }

    double get_imbalance() {
        if (bids.empty() || asks.empty()) return 0.5;
        double bid_vol = 0, ask_vol = 0;
        for(int i=0; i<std::min((size_t)5, bids.size()); i++) bid_vol += bids[i].quantity;
        for(int i=0; i<std::min((size_t)5, asks.size()); i++) ask_vol += asks[i].quantity;
        return bid_vol / (bid_vol + ask_vol);
    }

    double get_best_bid() { return bids.empty() ? 0.0 : bids[0].price; }
    double get_best_ask() { return asks.empty() ? 0.0 : asks[0].price; }
};

// --- 4. RISK MANAGER ---
class RiskManager {
private:
    const double MAX_ORDER_VALUE = 2000.0; 
    const double MAX_POSITION = 0.01;      
    double current_position = 0.0; 

public:
    bool check_order(const std::string& side, double price, double quantity) {
        double notional_value = price * quantity;
        if (notional_value > MAX_ORDER_VALUE) {
            std::cout << "[RISK REJECT] Value $" << notional_value << " too high." << std::endl;
            return false;
        }

        double projected_position = current_position;
        if (side == "BUY") projected_position += quantity;
        else projected_position -= quantity;

        if (std::abs(projected_position) > MAX_POSITION) {
            std::cout << "[RISK REJECT] Position " << projected_position << " exceeds limit." << std::endl;
            return false;
        }
        return true;
    }

    void update_position(const std::string& side, double quantity) {
        if (side == "BUY") current_position += quantity;
        else current_position -= quantity;
        std::cout << "[RISK] New Position: " << current_position << " BTC" << std::endl;
    }
};

// --- 5. EXECUTION GATEWAY ---
class ExecutionGateway {
public:
    long long send_order(const std::string& side, double price, double quantity) {
        auto start = std::chrono::steady_clock::now();
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
            "{\"symbol\":\"BTCUSD\",\"side\":\"%s\",\"type\":\"LIMIT\",\"quantity\":\"%.4f\",\"price\":\"%.2f\"}", 
            side.c_str(), quantity, price);
        
        // Fixed Busy Wait to avoid compiler warnings
        volatile int check = 0;
        for(int i=0; i<100; i++) check = i; 
        
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }
};

// --- 6. HTTP SNAPSHOT CLIENT ---
void fetch_snapshot(net::io_context& ioc, ssl::context& ctx, OrderBook& book) {
    try {
        tcp::resolver resolver{ioc};
        beast::ssl_stream<tcp::socket> stream{ioc, ctx};
        auto const results = resolver.resolve("api.binance.us", "443");
        net::connect(stream.next_layer(), results.begin(), results.end());
        stream.handshake(ssl::stream_base::client);
        http::request<http::string_body> req{http::verb::get, "/api/v3/depth?symbol=BTCUSD&limit=1000", 11};
        req.set(http::field::host, "api.binance.us");
        req.set(http::field::user_agent, "HFT-Client/1.0");
        http::write(stream, req);
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);
        simdjson::dom::parser parser;
        simdjson::dom::element doc = parser.parse(res.body());
        simdjson::dom::array bids = doc["bids"]; 
        simdjson::dom::array asks = doc["asks"]; 
        book.load_snapshot(bids, asks);
        beast::error_code ec;
        stream.shutdown(ec);
    } catch (std::exception const& e) {
        std::cerr << "Snapshot Error: " << e.what() << std::endl;
    }
}

// --- 7. MAIN ENGINE ---
int main() {
    try {
        alignas(std::max_align_t) std::array<std::byte, 1024 * 1024> memory_buffer; 
        std::pmr::monotonic_buffer_resource pool{memory_buffer.data(), memory_buffer.size()};

        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};
        ctx.set_default_verify_paths();
        
        OrderBook book(&pool);
        ExecutionGateway gateway;
        RiskManager risk;

        // --- DATA RECORDER SETUP ---
        std::ofstream log_file("market_data.log", std::ios::app);
        if (!log_file.is_open()) std::cerr << "[WARNING] Failed to open log file!" << std::endl;
        else std::cout << "[SYSTEM] Recording Market Data to market_data.log..." << std::endl;

        std::cout << "[SYSTEM] Fetching HTTP Snapshot..." << std::endl;
        fetch_snapshot(ioc, ctx, book);
        std::cout << "[SYSTEM] Snapshot Loaded. Connecting to Stream..." << std::endl;

        tcp::resolver resolver{ioc};
        auto const results = resolver.resolve("stream.binance.us", "9443");
        websocket::stream<beast::ssl_stream<tcp::socket>> ws{ioc, ctx};
        net::connect(beast::get_lowest_layer(ws), results);
        ws.next_layer().handshake(ssl::stream_base::client);
        ws.set_option(websocket::stream_base::decorator([](websocket::request_type& req) {req.set(http::field::user_agent, "HFT-Client/1.0");}));
        ws.handshake("stream.binance.us:9443", "/ws/btcusd@depth");
        
        simdjson::dom::parser parser;
        if (parser.allocate(64000) != simdjson::SUCCESS) std::cerr << "Memory allocation failure" << std::endl;
        beast::flat_buffer buffer;
        
        int cooldown = 0;
        int count = 0;
        double trade_qty = 0.002; 

        while(true) {
            ws.read(buffer);
            auto start_time = std::chrono::steady_clock::now();
            auto data_str = beast::buffers_to_string(buffer.data());

            // --- RECORDING ---
            log_file << data_str << "\n";
            
            simdjson::dom::element doc = parser.parse(data_str);
            simdjson::dom::array bids = doc["b"];
            simdjson::dom::array asks = doc["a"];

            for (simdjson::dom::array level : bids) book.update_bid(fast_atof(level.at(0)), fast_atof(level.at(1)));
            for (simdjson::dom::array level : asks) book.update_ask(fast_atof(level.at(0)), fast_atof(level.at(1)));

            auto end_time = std::chrono::steady_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
            buffer.consume(buffer.size());

            // Strategy
            if (cooldown > 0) cooldown--;
            if (cooldown == 0) {
                double imbalance = book.get_imbalance();

                if (book.get_best_ask() > book.get_best_bid()) {
                    std::string signal_side = "";
                    double signal_price = 0.0;

                    if (imbalance > 0.8) {
                        signal_side = "BUY";
                        signal_price = book.get_best_bid();
                    } else if (imbalance < 0.2) {
                        signal_side = "SELL";
                        signal_price = book.get_best_ask();
                    }

                    if (!signal_side.empty()) {
                        if (risk.check_order(signal_side, signal_price, trade_qty)) {
                            long long exec_time = gateway.send_order(signal_side, signal_price, trade_qty);
                            risk.update_position(signal_side, trade_qty);
                            std::cout << "[EXEC] " << signal_side << " | Latency: " << latency << "ns" << std::endl;
                            cooldown = 2000;
                        } else {
                            cooldown = 5000; 
                        }
                    }
                }
            }
            
            count++;
            if (count % 2000 == 0) std::cout << "Processed " << count << " updates." << std::endl;
        }

    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}