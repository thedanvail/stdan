#pragma once

#include <cassert>
#include <cstddef>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace stdan::memory {
    // Lambda magic to just get the page size without worrying about
    // a function we'll never really use
    static const std::size_t PAGE_SIZE = []() {
#ifdef _WIN32
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return sysInfo.dwPageSize;
#else
        return (std::size_t)sysconf(_SC_PAGESIZE);
#endif
    }();


    // TODO: Time to start breaking these up.
    // OutOfBounds shouldn't be an alloc error unless it's 
    // out of the bounds of the memory we're allocating.
    // I shouldn't reuse it in containers.
    enum class alloc_error {
        NullPointerArenaArg,
        ZeroSize,
        BadAlignment,
        NotEnoughMemory,
        OffsetOverflow,
        OutOfBounds,
        CouldNotCommitMemory,
        CouldNotReserveMemory,
        ReserveSizeOverflow
    };
}
