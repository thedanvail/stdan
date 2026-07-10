#pragma once

#include <cassert>
#include <cstddef>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace stdan::memory {
    /// The operating-system page size, or zero when it could not be queried.
    /// `inline` ensures the query and value are shared by all translation units.
    inline const std::size_t PAGE_SIZE = []() -> std::size_t {
#ifdef _WIN32
        SYSTEM_INFO sys_info{};
        GetSystemInfo(&sys_info);
        return static_cast<std::size_t>(sys_info.dwPageSize);
#else
        const long page_size = sysconf(_SC_PAGESIZE);
        return page_size > 0 ? static_cast<std::size_t>(page_size) : 0;
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
        CouldNotReleaseMemory,
        CouldNotDeterminePageSize,
        ReserveSizeOverflow
    };
}
