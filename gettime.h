#ifndef GETTIME_H
#define GETTIME_H

#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#else
#include <sys/time.h>
#endif

// The first-generation Apple TV compiler has no std::chrono.
inline uint64_t GetMicroseconds()
{
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (frequency.QuadPart == 0)
        QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(
        (double)counter.QuadPart * 1000000.0 / (double)frequency.QuadPart
    );
#elif defined(__APPLE__)
    static mach_timebase_info_data_t timebase = { 0, 0 };
    if (timebase.denom == 0)
        mach_timebase_info(&timebase);
    return (uint64_t)(
        (double)mach_absolute_time()
        * (double)timebase.numer
        / (double)timebase.denom
        / 1000.0
    );
#else
    struct timeval value;
    gettimeofday(&value, 0);
    return (uint64_t)value.tv_sec * (uint64_t)1000000
        + (uint64_t)value.tv_usec;
#endif
}

inline uint64_t GetMilliseconds()
{
    return GetMicroseconds() / (uint64_t)1000;
}

#endif
