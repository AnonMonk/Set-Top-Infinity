#ifndef GETTIME_H
#define GETTIME_H

#include <chrono>
#include <cstdint>

std::uint64_t GetMilliseconds()
{
    using namespace std::chrono;

    return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}

#endif