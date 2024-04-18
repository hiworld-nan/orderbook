#pragma once

// #include <x86intrin.h>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include "util.h"

struct TimeConstant {
    static constexpr uint64_t skNsPerUs = 1'000ul;
    static constexpr uint64_t skUsPerMs = 1'000ul;
    static constexpr uint64_t skNsPerMs = 1'000'000ul;

    static constexpr uint64_t skMsPerSecond = 1'000ul;
    static constexpr uint64_t skUsPerSecond = 1'000'000ul;
    static constexpr uint64_t skNsPerSecond = 1'000'000'000ul;
};

// note: cpu-migrantions cause accuration loss or even errors
//  so should pin thread to a single core and set cstate=0 and isolate the core
struct TscClock {
    static constexpr uint32_t kCalibrateLoopCnt = 71;
    static constexpr uint32_t kPauseMultiplier = 17;

    static TscClock& getInstance() {
        static TscClock clockInstance;
        return clockInstance;
    }

    void show() {
        std::cout << "ticksPerSecond:" << ticksPerSecond_ << std::endl;
        std::cout << "nsPerTick:" << nsPerTick_ << std::endl;
        std::cout << "ticksPerNs:" << ticksPerNs_ << std::endl;
        std::cout << "delayNsOffsetTicks_:" << delayNsOffsetTicks_ << std::endl;
    }

    void calibrate(uint32_t loopCnt = kCalibrateLoopCnt) {
        loopCnt = (loopCnt < kCalibrateLoopCnt) ? kCalibrateLoopCnt : loopCnt;
        calibrateTsc(loopCnt);
        calibrateDelayNsOffset(loopCnt);
    }

    uint64_t rdNs() const { return tsc2Ns(rdTsc()); }
    uint64_t rdTsc() const { return __builtin_ia32_rdtsc(); };

    inline uint64_t tsc2Ns(uint64_t tsc) const { return static_cast<uint64_t>(tsc * nsPerTick_); }
    inline uint64_t tsc2Sec(uint64_t tsc) const { return static_cast<uint64_t>(tsc / ticksPerSecond_); }

    void delayCycles(uint64_t cycles) {
        const uint64_t endTick = rdTsc() + cycles;
        while (((int64_t)endTick - (int64_t)rdTsc()) > 0) {
            __builtin_ia32_pause();
        }
    }

    // todo: Implement delayNs using umwait/tpause.
    NoOptimize void delayNs(uint64_t ns) {
        const uint64_t nowTick = rdTsc();
        const uint64_t endTick = nowTick + ns * ticksPerNs_ - delayNsOffsetTicks_;
        if (nowTick >= endTick) {
            return;
        }

        while (((int64_t)endTick - (int64_t)rdTsc()) > 0) {
            __builtin_ia32_pause();
        }
    }

   private:
    TscClock() = default;
    ~TscClock() = default;

    NoOptimize void calibrateTsc(uint32_t loopCnt = kCalibrateLoopCnt) {
        uint64_t billion = TimeConstant::skNsPerSecond;
        std::timespec beginTime = {0, 0}, endTime = {0, 0};

        uint64_t intervalTsc = 0, intervalNs = 0;
        uint64_t deltaInitial = 0, deltaTerminate = 0, deltaTotal = 0, deltaMin = ~0;
        uint64_t initialBeginTsc = 0, initialEndTsc = 0, terminateBeginTsc = 0, terminateEndTsc = 0;
        for (uint32_t i = 0; i < loopCnt; i++) {
            initialBeginTsc = rdTsc();
            clock_gettime(CLOCK_MONOTONIC_RAW, &beginTime);
            initialEndTsc = rdTsc();

            for (uint64_t j = 0; j < TimeConstant::skNsPerMs * kPauseMultiplier; j++) {
                __builtin_ia32_pause();
            }

            terminateBeginTsc = rdTsc();
            clock_gettime(CLOCK_MONOTONIC_RAW, &endTime);
            terminateEndTsc = rdTsc();

            deltaInitial = initialEndTsc - initialBeginTsc;
            deltaTerminate = terminateEndTsc - terminateBeginTsc;
            deltaTotal = deltaInitial + deltaTerminate;

            if (deltaTotal < deltaMin) {
                deltaMin = deltaTotal;
                intervalTsc = terminateBeginTsc - initialEndTsc;
                intervalNs = (endTime.tv_sec - beginTime.tv_sec) * billion + endTime.tv_nsec - beginTime.tv_nsec;

                ticksPerNs_ = intervalTsc / static_cast<double>(intervalNs);
                nsPerTick_ = static_cast<double>(intervalNs) / intervalTsc;
                ticksPerSecond_ = intervalTsc / static_cast<double>(intervalNs) * billion;
            }
        }
    }

    NoOptimize void calibrateDelayNsOffset(uint32_t loopCnt = kCalibrateLoopCnt) {
        delayNsOffsetTicks_ = 0.0;
        const uint64_t cnt = loopCnt * kPauseMultiplier;

        uint64_t beginTick = rdTsc();
        for (uint64_t i = 0; i < cnt; i++) {
            delayNs(0);
        }
        uint64_t endTick = rdTsc();
        delayNsOffsetTicks_ = static_cast<double>(endTick - beginTick) / cnt;
    }

   private:
    alignas(kDefaultCacheLineSize) double ticksPerSecond_ = 1.0;
    double nsPerTick_ = 1.0;
    double ticksPerNs_ = 1.0;
    double delayNsOffsetTicks_ = 0.0;
};