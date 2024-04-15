#pragma once

// #include <x86intrin.h>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <thread>
#include "util.h"

struct alignas(kDefaultCacheLineSize) TimeConstant {
    static inline double skNsPerTick = 1ul;
    static inline double skTickPerNs = 1ul;
    // about 10 ticks per pause for intel cpu
    static inline uint64_t skTicksPerPause = 10ul;
    static inline uint64_t skTicksPerSecond = 1'000'000'000ul;

    static constexpr uint64_t skNsPerUs = 1'000ul;
    static constexpr uint64_t skUsPerMs = 1'000ul;
    static constexpr uint64_t skNsPerMs = 1'000'000ul;

    static constexpr uint64_t skMsPerSecond = 1'000ul;
    static constexpr uint64_t skUsPerSecond = 1'000'000ul;
    static constexpr uint64_t skNsPerSecond = 1'000'000'000ul;
};

static ForceInline uint64_t rdtsc() {
    uint32_t aux = 0;
    union {
        uint64_t cycle;
        struct {
            uint32_t lo;
            uint32_t hi;
        };
    } tsc = {0};

    asm volatile("rdtsc\n" : "=a"(tsc.lo), "=d"(tsc.hi), "=c"(aux)::);
    return tsc.cycle;
    // return _rdtsc();
}

#pragma GCC push_options
#pragma GCC optimize("O0")
template <uint32_t LOOP = 371>
static NoInline uint64_t getTicksOfPause() {
    int32_t i = 0;
    uint64_t beginTick = 0, endTick = 0;

    // warm-up
    do {
        beginTick = rdtsc();
        asm volatile("pause" :::);
        endTick = rdtsc();
    } while (i++ < LOOP);

    beginTick = rdtsc();
    for (i = 0; i < LOOP; i++) {
        asm volatile("pause" :::);
        //_mm_pause();
    }
    endTick = rdtsc();
    return TimeConstant::skTicksPerPause = (endTick - beginTick) / (i + 1);
}
#pragma GCC pop_options

template <uint32_t LOOP = 371>
static NoInline uint64_t calibrateTsc() {
    std::timespec ts, te;
    uint64_t tss = 0, tse = 0, tes = 0, tee = 0;
    uint64_t deltaStart = 0, deltaEnd = 0, deltaMin = ~0;

    const long durationUs = 20000 * TimeConstant::skNsPerUs;
    struct timespec sleep_time = {0, durationUs};
    double freq = 0.0, billion = TimeConstant::skNsPerSecond;
    for (uint32_t i = 0; i < LOOP; i++) {
        tss = rdtsc();
        clock_gettime(CLOCK_MONOTONIC, &ts);
        tse = rdtsc();

        nanosleep(&sleep_time, nullptr);

        tes = rdtsc();
        clock_gettime(CLOCK_MONOTONIC, &te);
        tee = rdtsc();

        deltaStart = tse - tss;
        deltaEnd = tee - tes;
        if ((deltaStart + deltaEnd) < deltaMin) {
            freq = (te.tv_sec - ts.tv_sec) * billion + te.tv_nsec - ts.tv_nsec;
            freq = (tes - tse) * (billion / freq);
            deltaMin = deltaStart + deltaEnd;
        }
    }

    TimeConstant::skNsPerTick = TimeConstant::skNsPerSecond / freq;
    TimeConstant::skTickPerNs = freq / TimeConstant::skNsPerSecond;
    return TimeConstant::skTicksPerSecond = static_cast<uint64_t>(freq);
}

static ForceInline uint64_t ns2Tsc(const uint64_t ns) { return static_cast<uint64_t>(ns * TimeConstant::skTickPerNs); }
static ForceInline uint64_t tsc2Ns(const uint64_t tsc) {
    return static_cast<uint64_t>(tsc * TimeConstant::skNsPerTick);
}
