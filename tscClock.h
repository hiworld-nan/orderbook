#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <thread>
#include "util.h"

using namespace std;

static inline uint64_t skTicksPerSecond = 1'000'000'000ul;

static constexpr uint64_t skUsPerMs = 1'000ul;
static constexpr uint64_t skNsPerMs = 1'000'000ul;
static constexpr uint64_t skNsPerUs = 1'000ul;

static constexpr uint64_t skNsPerSecond = 1'000'000'000ul;
static constexpr uint64_t skUsPerSecond = 1'000'000ul;
static constexpr uint64_t skMsPerSecond = 1'000ul;

static uint64_t rdtsc() { return __builtin_ia32_rdtsc(); }

template <uint32_t LOOP = 71>
static NoInline uint64_t calibrateTsc() {
    std::timespec ts, te;
    uint64_t tss = 0, tse = 0, tes = 0, tee = 0, tscFreq = 0;
    uint64_t deltaStart = 0, deltaEnd = 0, deltaMin = ~0;

    uint32_t billion = skNsPerSecond;
    for (uint32_t i = 0; i < LOOP; i++) {
        tss = rdtsc();
        std::timespec_get(&ts, TIME_UTC);
        tse = rdtsc();

        std::this_thread::sleep_for(std::chrono::microseconds(20000));

        tes = rdtsc();
        std::timespec_get(&te, TIME_UTC);
        tee = rdtsc();

        deltaStart = tse - tss;
        deltaEnd = tee - tes;
        if ((deltaStart + deltaEnd) < deltaMin) {
            tscFreq = (te.tv_sec - ts.tv_sec) * billion + te.tv_nsec - ts.tv_nsec;
            tscFreq = (tes - tse) * (billion / (double)tscFreq);
            deltaMin = deltaStart + deltaEnd;
        }
    }

    return skTicksPerSecond = tscFreq;
}

static ForceInline uint64_t ns2Tsc(const uint64_t ns) { return ((ns)*skTicksPerSecond / skNsPerSecond); }
static ForceInline uint64_t tsc2Ns(const uint64_t tsc) { return ((tsc)*skNsPerSecond / skTicksPerSecond); }
