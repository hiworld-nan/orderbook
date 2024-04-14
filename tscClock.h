#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <thread>
#include "util.h"

static inline uint64_t skTicksPerSecond = 1'000'000'000ul;

static constexpr uint64_t skUsPerMs = 1'000ul;
static constexpr uint64_t skNsPerMs = 1'000'000ul;
static constexpr uint64_t skNsPerUs = 1'000ul;

static constexpr uint64_t skNsPerSecond = 1'000'000'000ul;
static constexpr uint64_t skUsPerSecond = 1'000'000ul;
static constexpr uint64_t skMsPerSecond = 1'000ul;

static inline uint64_t rdtsc() {
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
}

template <uint32_t LOOP = 71>
static NoInline uint64_t calibrateTsc() {
    std::timespec ts, te;
    uint64_t tss = 0, tse = 0, tes = 0, tee = 0;
    uint64_t deltaStart = 0, deltaEnd = 0, deltaMin = ~0;

    const long durationUs = 20000 * skNsPerUs;
    struct timespec sleep_time = {0, durationUs};
    double freq = 0.0, billion = skNsPerSecond;
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

    return skTicksPerSecond = static_cast<uint64_t>(freq);
}

static ForceInline uint64_t ns2Tsc(const uint64_t ns) { return ((ns)*skTicksPerSecond / skNsPerSecond); }
static ForceInline uint64_t tsc2Ns(const uint64_t tsc) { return ((tsc)*skNsPerSecond / skTicksPerSecond); }
