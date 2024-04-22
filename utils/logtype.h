#pragma once // This is better in case #ifndef LOG_TYPE conflicts with the name of the enum

// include other header files so when we include this header file, these also get included
#include <cstdlib> // for size_t
#include "macros.h"
#include "lock_free_queue.h"
#include "thread_utils.h"
#include "time_utils.h"

namespace Common {
    /*
        This will represent the log entry that will hold information that the main thread will send to the I/O logging thread through LFQ
    */

    constexpr size_t LOG_QUEUE_SIZE = 8 * 1024 * 1024;

    enum class LogType : int8_t  {
        CHAR = 0,
        INTEGER = 1, LONG_INTEGER = 2, LONG_LONG_INTEGER = 3,
        UNSIGNED_INTEGER = 4, UNSIGNED_LONG_INTEGER = 5, UNSIGNED_LONG_LONG_INTEGER = 6,
        FLOAT = 7, DOUBLE = 8

    };

    struct LogElement {
        LogType type = LogType::CHAR; // default value of a CHAR
        union {
            char c; 
            int i; long l; long long ll; 
            unsigned u; unsigned long ul; unsigned long long ull; 
            float f; double d;
        } u; // can hold many different data types; in a union, only one can be active at any given time
    };
    
}