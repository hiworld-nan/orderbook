#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include "tscClock.h"

int32_t main(int32_t argc, char* argv[]) {
    int32_t loopCnt = std::atoi(argv[1]);

    uint64_t startTsc = 0, endTsc = 0;
    TscClock& clock = TscClock::getInstance();
    clock.calibrate(371);
    clock.show();

    std::vector<int32_t> vecDelayNs = {
        0,     1,     5,     10,    15,    20,    25,    30,    40,    50,    70,    80,    90,    100,
        150,   200,   250,   300,   350,   400,   450,   500,   550,   600,   650,   700,   750,   800,
        850,   900,   950,   1000,  1500,  2000,  2500,  3000,  3500,  4000,  4500,  5000,  5500,  6000,
        6500,  7000,  7500,  8000,  8500,  9000,  10000, 15000, 20000, 25000, 30000, 35000, 40000, 45000,
        50000, 55000, 60000, 65000, 70000, 75000, 80000, 85000, 90000, 95000, 100000};

    for (auto v : vecDelayNs) {
        startTsc = clock.rdTsc();
        for (auto i = 0; i < loopCnt; i++) {
            clock.delayNs(v);
        }
        endTsc = clock.rdTsc();
        std::cout << "delay" << v << "Ns " << "actual:" << clock.tsc2Ns(endTsc - startTsc) / loopCnt << "ns"
                  << std::endl;
    }

    return 0;
}