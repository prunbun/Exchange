#pragma once

#include <cstdint>
#include <mach/mach_time.h>

namespace Common {

    // store upper bits of the "timestamp counter" variable in hi and the lower ones in lo
    // this variable stores the number of clock cycles since the last reset or set epoch
    // inline auto rdtsc() noexcept {
    //     uint32_t lo, hi;

    //     __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    //     return ((uint64_t) hi << 32) | lo;
    // }

    // rdtsc doesn't work on Mac since it is an x86 command :/
    inline auto rdtsc() noexcept {
        return mach_absolute_time();
    }

}

#define START_MEASURE(TAG) const auto TAG = Common::rdtsc()

#define END_MEASURE(TAG, LOGGER) \
    do { \
        const auto end = Common::rdtsc(); \
        LOGGER.log("% RDTSC "#TAG" %\n", Common::getCurrentTimeStr(&time_str), (end - TAG)); \
    } while (false)

#define TTT_MEASURE(TAG, LOGGER) \
    do { \
        const auto TAG = Common::getCurrentNanos(); \
        LOGGER.log("% TTT "#TAG" %\n", Common::getCurrentTimeStr(&time_str), TAG); \
    } while (false)