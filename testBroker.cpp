#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
#include "broker.h"
#include "orderBookInlinePrint.h"

void usage() { std::cout << "usage: ./tob number_of_orders" << std::endl; }

int32_t main(int32_t argc, char* argv[]) {
    if (argc != 2) {
        usage();
        return -1;
    }

    TscClock& clock = TscClock::getInstance();
    clock.calibrate();
    std::cout << clock << std::endl;

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
            o.coid_ = o.createTimeNs_ = clock.rdNs();

            if (0 == v) {
                o.price_ = (constV - i) % 100 + 1;
                o.remainQty_ = o.qty_ = i % 10 + 1;
                o.side_ = QuoteType::Buy;
            } else {
                o.price_ = (constV + i) % 100 + 100;
                o.remainQty_ = o.qty_ = i % 10 + 1;
                o.side_ = QuoteType::Sell;
            }
            beginTick = clock.rdTsc();
            broker.insertOrder(o);
            // broker.getOrderBook(zob);
            // showOrderBook(zob);
            endTick = clock.rdTsc();
            totalTick += endTick - beginTick;
        }

        std::cout << "build orderbook in :" << clock.tsc2Ns(totalTick) / constFv << "ns" << std::endl;

        beginTick = clock.rdTsc();
        broker.getOrderBook(zob);
        endTick = clock.rdTsc();
        showOrderBook(zob);
        std::cout << "latency of getOrderBook is: " << clock.tsc2Ns(endTick - beginTick) << "ns" << std::endl;
        std::cout << std::endl << std::endl;
    }

    {
        totalTick = 0;
        for (auto i = 0; i < constV; i++) {
            Order o;
            const int32_t v = i & 1;
            o.type_ = OrderType::Limit;
            o.coid_ = o.createTimeNs_ = clock.rdNs();
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
            beginTick = clock.rdTsc();
            broker.insertOrder(o);
            // Preventing Optimization
            // broker.getOrderBook(zob);
            endTick = clock.rdTsc();
            totalTick += endTick - beginTick;

            /*{
                beginTick = clock.rdTsc();
                broker.getOrderBook(zob);
                endTick = clock.rdTsc();
                showOrderBook(zob);
                std::cout << "delay for getOrderBook<10>: " << clock.tsc2Ns(endTick - beginTick) << "ns" <<
            std::endl;
            }*/
        }

        std::cout << "each order is matched in :" << clock.tsc2Ns(totalTick) / constFv << "ns" << std::endl;

        beginTick = clock.rdTsc();
        broker.getOrderBook(zob);
        endTick = clock.rdTsc();
        showOrderBook(zob);
        std::cout << "latency of getOrderBook is: " << clock.tsc2Ns(endTick - beginTick) << "ns" << std::endl;
        std::cout << std::endl << std::endl;
    }

    return 0;
}

// g++ -Ofast -o tob testOrderBook.cpp -I./
