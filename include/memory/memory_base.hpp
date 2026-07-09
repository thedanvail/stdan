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
    // I should probably move this into some sort of `base.hpp` file
    // since I'll likely use this across many memory modules, but for now,
    // this'll do.
    static const std::size_t PAGE_SIZE = []() {
#ifdef _WIN32
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return sysInfo.dwPageSize;
#else
        return (std::size_t)sysconf(_SC_PAGESIZE);
#endif
    }();

    enum class alloc_error {
        NullPointerArenaArg,
        ZeroSize,
        BadAlignment,
        NotEnoughMemory,
        OffsetOverflow,
        CouldNotCommitMemory,
        CouldNotReserveMemory,
        ReserveSizeOverflow
    };
}
