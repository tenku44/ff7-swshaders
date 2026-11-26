#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>

#include "lib.hpp"

namespace rh {

inline void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

inline void Log(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n <= 0) return;
    if (n >= static_cast<int>(sizeof(buf))) n = static_cast<int>(sizeof(buf)) - 1;
    svcOutputDebugString(buf, static_cast<uint64_t>(n));
}

}  // namespace rh
