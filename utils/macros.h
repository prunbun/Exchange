#ifndef MACROS_H
#define MACROS_H

#include <iostream>
#include <cstring>

using std::cerr;
using std::string;
using std::endl;

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)


inline auto ASSERT(bool cond, const string &message) noexcept {
    if (UNLIKELY(!cond)) {
        cerr << "ASSERT: " << message << endl;
        exit(EXIT_FAILURE);
    }
}

inline auto FATAL(const string &message) noexcept {
    cerr << "FATAL: " << message << endl;

    exit(EXIT_FAILURE);
}

#endif