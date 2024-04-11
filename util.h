#pragma once

static constexpr int32_t kDefaultCacheLineSize = 64;

#ifndef ForceInline
#define ForceInline __attribute__((always_inline)) inline
#endif

#ifndef NoInline
#define NoInline __attribute__((noinline))
#endif

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif