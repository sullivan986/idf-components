#ifndef _Z_BASE_HPP_
#define _Z_BASE_HPP_

#include <chrono>
#include <thread>

using std::chrono::milliseconds;
using std::this_thread::sleep_for;

inline void z_sleep_ms(int64_t time_ms)

{
    std::this_thread::sleep_for(std::chrono::milliseconds(time_ms));
}

#endif