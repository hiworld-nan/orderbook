#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
#include "broker.h"
#include "orderBookInlinePrint.h"

using namespace std;

void usage() { std::cout << "usage: ./tob number_of_orders" << std::endl; }

int32_t main(int32_t argc, char* argv[]) {
    if (argc != 2) {
        usage();
        return -1;
    }

    calibrateTsc();

    Broker broker;
    Orderbook<10> zob;
    uint64_t beginTick = 0, endTick = 0, totalTick = 0;
    const int32_t constV = std::stoull(argv[1]);
    const float constFv = static_cast<float>(constV);

    {
        for (auto i = 0; i < constV; i++) {
            Order o;
            const int32_t v = i & 1;
            o.type_ = OrderType::Limit;
            o.coid_ = o.createTimeNs_ = tsc2Ns(rdtsc());
            if (0 == v) {
                o.price_ = (constV - i) % 100 + 1;
                o.remainQty_ = o.qty_ = i % 10 + 1;
                o.side_ = QuoteType::Buy;
            } else {
                o.price_ = (constV + i) % 100 + 100;
                o.remainQty_ = o.qty_ = i % 10 + 1;
                o.side_ = QuoteType::Sell;
            }
            beginTick = rdtsc();
            broker.insertOrder(o);
            // broker.getOrderBook(zob);
            // showOrderBook(zob);
            endTick = rdtsc();
            totalTick += endTick - beginTick;
        }

        std::cout << "build orderbook in :" << tsc2Ns(totalTick) / constFv << "ns" << std::endl;

        beginTick = rdtsc();
        broker.getOrderBook(zob);
        endTick = rdtsc();
        showOrderBook(zob);
        std::cout << "the latency of getOrderBook is: " << tsc2Ns(endTick - beginTick) << "ns" << std::endl;
        std::cout << std::endl << std::endl;
    }

    {
        totalTick = 0;
        for (auto i = 0; i < constV; i++) {
            Order o;
            const int32_t v = i & 1;
            o.type_ = OrderType::Limit;
            o.coid_ = o.createTimeNs_ = tsc2Ns(rdtsc());
            if (0 == v) {
                o.price_ = (constV + i) % 100 + 100;
                o.remainQty_ = o.qty_ = i % 10 + 1;
                o.side_ = QuoteType::Buy;
            } else {
                o.price_ = (constV - i) % 100 + 1;
                o.remainQty_ = o.qty_ = i % 10 + 1;
                o.side_ = QuoteType::Sell;
            }
            // std::cout << "insert order: " << o << std::endl;
            beginTick = rdtsc();
            broker.insertOrder(o);
            // Preventing Optimization
            // broker.getOrderBook(zob);
            endTick = rdtsc();
            totalTick += endTick - beginTick;

            /*{
                beginTick = rdtsc();
                broker.getOrderBook(zob);
                endTick = rdtsc();
                showOrderBook(zob);
                std::cout << "the delay for getOrderBook<10>: " << tsc2Ns(endTick - beginTick) << "ns" << std::endl;
            }*/
        }

        std::cout << "each order is matched in :" << tsc2Ns(totalTick) / constFv << "ns" << std::endl;

        beginTick = rdtsc();
        broker.getOrderBook(zob);
        endTick = rdtsc();
        showOrderBook(zob);
        std::cout << "the latency of getOrderBook is: " << tsc2Ns(endTick - beginTick) << "ns" << std::endl;
        std::cout << std::endl << std::endl;
    }

    return 0;
}

// g++ -Ofast -o tob testOrderBook.cpp -I./
