#include "arena.hpp"
#include "memory_base.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace stdan::memory
{
    namespace
    {
        constexpr int FILE_DESCRIPTOR = -1;
        constexpr int OFFSET = 0;
    }

    void arena_reset(arena* p_arena)
    { 
        if(p_arena == nullptr) { return; } 
        p_arena->current_offset = 0;
    }

    void arena_release(arena* p_arena)
    {
        if(p_arena == nullptr) { return; }
#ifdef _WIN32
        VirtualFree(p_arena->base_ptr, 0, MEM_RELEASE);
#else
        munmap(p_arena->base_ptr, p_arena->reserved_size);
#endif
        std::free(p_arena);
    }

    [[nodiscard]] std::expected<arena*, arena_alloc_error> create_arena(std::size_t reserve_size)
    {
        if(reserve_size == 0) { return std::unexpected(arena_alloc_error::ZeroSize); }
        arena* ret = static_cast<arena*>(std::malloc(sizeof(arena)));
        if(ret == nullptr) { return std::unexpected(arena_alloc_error::CouldNotReserveMemory); }
        
        // TODO: check for reserve size overflow
        reserve_size = (reserve_size + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;
        if(reserve_size > (SIZE_MAX - (PAGE_SIZE - 1))) { return std::unexpected(arena_alloc_error::ReserveSizeOverflow); }
#ifdef _WIN32
        void* block = VirtualAlloc(nullptr, reserve_size, MEM_RESERVE, PAGE_NOACCESS);
        if(block == nullptr)
        {
            std::free(ret);
            return std::unexpected(arena_alloc_error::MallocFailed);
        }
#else
        void* block = mmap(
                nullptr, 
                reserve_size, 
                PROT_NONE,
                (MAP_PRIVATE | MAP_ANONYMOUS),
                FILE_DESCRIPTOR, 
                OFFSET
        );
        if(block == MAP_FAILED)
        {
            std::free(ret);
            return std::unexpected(arena_alloc_error::CouldNotReserveMemory);
        }
#endif
        ret->base_ptr = static_cast<std::byte*>(block);
        ret->reserved_size = reserve_size;
        ret->committed_size = 0;
        ret->current_offset = 0;
        return ret;
    }

    [[nodiscard]] std::expected<std::byte*, arena_alloc_error> arena_alloc(arena* p_arena, std::size_t size, std::size_t alignment)
    {
        if(p_arena == nullptr) { return std::unexpected(arena_alloc_error::NullPointerArenaArg); }
        if(size == 0) { return std::unexpected(arena_alloc_error::ZeroSize); }

        // power of 2 check
        if(alignment == 0 || (alignment & (alignment - 1)) != 0)
        {
            return std::unexpected(arena_alloc_error::BadAlignment);
        }

        // Black magic alignment shenanigans
        std::size_t aligned_offset = (p_arena->current_offset + alignment - 1) & ~(alignment - 1);

        // overflow/underflow checks
        if(aligned_offset < p_arena->current_offset)
        { 
            return std::unexpected(arena_alloc_error::OffsetOverflow);
        }

        if(aligned_offset > p_arena->reserved_size)
        {
            return std::unexpected(arena_alloc_error::NotEnoughMemory);
        }

        if(size > p_arena->reserved_size - aligned_offset)
        { 
            return std::unexpected(arena_alloc_error::NotEnoughMemory);
        }

        std::size_t new_offset = aligned_offset + size;

        if(new_offset > p_arena->committed_size)
        {
            // Align the required commit size up to the nearest page
            std::size_t new_commit_target = (new_offset + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;
            std::size_t size_to_commit    = new_commit_target - p_arena->committed_size;
            std::byte* commit_start_addr  = p_arena->base_ptr + p_arena->committed_size;

#ifdef _WIN32
            if(!VirtualAlloc(commit_start_addr, size_to_commit, MEM_COMMIT, PAGE_READWRITE))
            { 
                return std::unexpected(arena_alloc_error::CouldNotCommitMemory);
            }
#else
            if(mprotect(commit_start_addr, size_to_commit, PROT_READ | PROT_WRITE) != 0)
            {
                return std::unexpected(arena_alloc_error::CouldNotCommitMemory);
            }
#endif
            p_arena->committed_size = new_commit_target;
        }
        
        std::byte* memory = p_arena->base_ptr + aligned_offset;
        p_arena->current_offset = new_offset;
        return memory;
    }
}
