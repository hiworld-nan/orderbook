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

struct TscClock {
    static constexpr uint32_t kCalibrateLoopCnt = 71;
    static constexpr uint32_t kPauseMultiplier = 17;
    static constexpr uint32_t kDelayOffsetMultiplier = 12345;

    static TscClock& getInstance() {
        static TscClock clockInstance;
        return clockInstance;
    }

    void show() {
        std::cout << "ticksPerSecond:" << ticksPerSecond_ << std::endl;
        std::cout << "nsPerTick:" << nsPerTick_ << std::endl;
        std::cout << "ticksPerNs:" << ticksPerNs_ << std::endl;
        std::cout << "delayNsOffsetTicks_:" << delayNsOffsetTicks_ << std::endl;
        std::cout << "delayNsOffsetNs:" << delayNsOffsetNs_ << std::endl;
    }

    void calibrate(uint32_t loopCnt = kCalibrateLoopCnt) {
        loopCnt = (loopCnt < kCalibrateLoopCnt) ? kCalibrateLoopCnt : loopCnt;
        calibrateTsc(loopCnt);
        calibrateDelayNsOffset(loopCnt);
    }

    uint64_t rdTsc() const {
        /*union {
            uint64_t cycle;
            struct {
                uint32_t lo;
                uint32_t hi;
            };
        } tsc = {0};

        asm volatile("rdtsc" : "=a"(tsc.lo), "=d"(tsc.hi)::);
        return tsc.cycle;*/
        return __builtin_ia32_rdtsc();
    };

    uint64_t rdNs() const { return tsc2Ns(rdTsc()); }
    inline uint64_t tsc2Ns(uint64_t tsc) const { return static_cast<uint64_t>(tsc * nsPerTick_); }
    inline uint64_t tsc2Sec(uint64_t tsc) const { return static_cast<uint64_t>(tsc / ticksPerSecond_); }

    void delayCycles(uint32_t cycles) {
        const uint64_t endTick = rdTsc() + cycles;
        while (rdTsc() < endTick) {
            // asm volatile("pause" :::);
            __builtin_ia32_pause();
        }
    }

// todo: Implement delayNs using umwait/tpause.
#pragma GCC push_options
#pragma GCC optimize("O0")
    void delayNs(uint32_t ns) {
        const uint64_t endTick = rdTsc() + ns * ticksPerNs_ - delayNsOffsetTicks_;
        while (rdTsc() < endTick) {
            // asm volatile("pause" :::);
            __builtin_ia32_pause();
        }
    }
#pragma GCC pop_options

   private:
    TscClock() = default;
    ~TscClock() = default;

#pragma GCC push_options
#pragma GCC optimize("O0")
    void calibrateTsc(uint32_t loopCnt = kCalibrateLoopCnt) {
        uint64_t billion = TimeConstant::skNsPerSecond;
        std::timespec beginTime = {0, 0}, endTime = {0, 0};

        uint64_t intervalTsc = 0, intervalNs = 0;
        uint64_t deltaInitial = 0, deltaTerminate = 0, deltaTotal = 0, deltaMin = ~0;
        uint64_t initialBeginTsc = 0, initialEndTsc = 0, terminateBeginTsc = 0, terminateEndTsc = 0;
        for (uint32_t i = 0; i < loopCnt; i++) {
            initialBeginTsc = rdTsc();
            clock_gettime(CLOCK_MONOTONIC_RAW, &beginTime);
            initialEndTsc = rdTsc();

            for (uint64_t i = 0; i < TimeConstant::skNsPerMs * kPauseMultiplier; i++) {
                // asm volatile("pause" :::);
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

    void calibrateDelayNsOffset(uint32_t loopCnt = kCalibrateLoopCnt) {
        delayNsOffsetTicks_ = 0.0;
        delayNsOffsetNs_ = 0.0;
        loopCnt = loopCnt * kPauseMultiplier * kDelayOffsetMultiplier;
        uint64_t beginTick = rdTsc();
        for (uint32_t i = 0; i < loopCnt; i++) {
            delayNs(1);
        }
        uint64_t endTick = rdTsc();
        delayNsOffsetTicks_ = static_cast<double>(endTick - beginTick) / loopCnt;
        delayNsOffsetNs_ = delayNsOffsetTicks_ * nsPerTick_;
    }
#pragma GCC pop_options

   private:
    alignas(kDefaultCacheLineSize) double ticksPerSecond_ = 1.0;
    double nsPerTick_ = 1.0;
    double ticksPerNs_ = 1.0;

    double delayNsOffsetTicks_ = 0.0;
    double delayNsOffsetNs_ = 0.0;
};