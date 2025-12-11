cmake_minimum_required(VERSION 3.15)
project(HFT_Trading_System)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "/opt/homebrew") 
include_directories("/opt/homebrew/include")
link_directories("/opt/homebrew/lib")

find_package(Boost REQUIRED) 
find_package(simdjson REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(OrderBookEngine orderbook.cpp)

if(Boost_FOUND)
    target_include_directories(OrderBookEngine PRIVATE ${Boost_INCLUDE_DIRS})
endif()

target_link_libraries(OrderBookEngine 
    PRIVATE 
    simdjson::simdjson
    OpenSSL::SSL 
    OpenSSL::Crypto
)

target_compile_options(OrderBookEngine PRIVATE -O3 -march=native -pthread)

# --- ADD BACKTESTER ---
add_executable(Backtester backtester.cpp)

# Reuse the same libraries (simdjson, etc)
target_link_libraries(Backtester 
    PRIVATE 
    simdjson::simdjson
)

# Use the same optimizations
target_compile_options(Backtester PRIVATE -O3 -march=native)