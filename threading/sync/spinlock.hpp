#pragma once

#include <atomic>
#include <unistd.h>

class SpinLock
{
public:

    void lock()
    {
        while(lock_flag.test_and_set(std::memory_order_acquire))
            usleep(250);
    }

    void unlock()
    {
        lock_flag.clear(std::memory_order_release);
    }

private:
    // guaranteed by standard to be lock free!
    std::atomic_flag lock_flag = ATOMIC_FLAG_INIT;
};
