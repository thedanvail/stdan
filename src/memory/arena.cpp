#include "memory/arena.hpp"
#include "memory/memory_base.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>

namespace stdan::memory {
    namespace {
        constexpr int FILE_DESCRIPTOR = -1;
        constexpr int OFFSET = 0;
    }

    // TODO: Perhaps we should zero-out everything upon reset?
    void arena_reset(arena* p_arena) noexcept {
        if(p_arena == nullptr) { return; }
        p_arena->current_offset = 0;
    }

    /// On the incredibly rare occasion that the OS cannot free the memory,
    /// we'll return the issue to the caller.
    std::expected<void, alloc_error> arena_release(arena* p_arena) noexcept {
        if(p_arena == nullptr) { return {}; }
        bool released;
#ifdef _WIN32
        released = VirtualFree(p_arena->base_ptr, 0, MEM_RELEASE) != 0;
#else
        released = munmap(p_arena->base_ptr, p_arena->reserved_size) == 0;
#endif
        // Do not free the metadata until we're sure this has succeeded
        if(!released) {
            return std::unexpected(alloc_error::CouldNotReleaseMemory);
        }
        std::free(p_arena);
        return {};
    }

    /// Bog-standard deleter/free-er (that sounds weird)
    /// This is my best attempt to effectively do GC with the
    /// unique pointer.
    void arena_deleter::operator()(arena* p_arena) const noexcept {
        static_cast<void>(arena_release(p_arena)); // NOLINT(bugprone-unused-return-value)
    }

    [[nodiscard]] std::expected<arena_owner, alloc_error> create_arena(std::size_t reserve_size) {
        if(reserve_size == 0) { return std::unexpected(alloc_error::ZeroSize); }
        if(PAGE_SIZE == 0) {
            return std::unexpected(alloc_error::CouldNotDeterminePageSize);
        }
        arena* ret = static_cast<arena*>(std::malloc(sizeof(arena)));
        if(ret == nullptr) { return std::unexpected(alloc_error::CouldNotReserveMemory); }

        // Check for reserve size overflow before rounding up to the nearest page boundary.
        if(reserve_size > (SIZE_MAX - (PAGE_SIZE - 1))) {
            std::free(ret);
            return std::unexpected(alloc_error::ReserveSizeOverflow);
        }
        reserve_size = (reserve_size + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;
#ifdef _WIN32
        void* block = VirtualAlloc(nullptr, reserve_size, MEM_RESERVE, PAGE_NOACCESS);
        if(block == nullptr) {
            std::free(ret);
            return std::unexpected(alloc_error::CouldNotReserveMemory);
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
        if(block == MAP_FAILED) {
            std::free(ret);
            return std::unexpected(alloc_error::CouldNotReserveMemory);
        }
#endif
        ret->base_ptr       = static_cast<std::byte*>(block);
        ret->reserved_size  = reserve_size;
        ret->committed_size = 0;
        ret->current_offset = 0;
        return arena_owner(ret);
    }

    // Assumed invariants, enforced at arena creation:
    //   - PAGE_SIZE > 0 and is a power of two
    //   - p_arena->reserved_size is a multiple of PAGE_SIZE
    //   - p_arena->base_ptr is page-aligned (guaranteed by the OS)
    //   - p_arena->current_offset <= p_arena->committed_size <= p_arena->reserved_size
    [[nodiscard]] std::expected<std::byte*, alloc_error>
    arena_alloc(arena* p_arena, std::size_t size, std::size_t alignment) {
        if(p_arena == nullptr) {
            return std::unexpected(alloc_error::NullPointerArenaArg);
        }
        if(size == 0) {
            return std::unexpected(alloc_error::ZeroSize);
        }
        // Must be a nonzero power of two. This covers any alignas(N) type,
        // including over-aligned ones, as long as the caller passes alignof(T).
        if(alignment == 0 || (alignment & (alignment - 1)) != 0) {
            return std::unexpected(alloc_error::BadAlignment);
        }

        assert(PAGE_SIZE != 0 && (PAGE_SIZE & (PAGE_SIZE - 1)) == 0);
        assert(p_arena->reserved_size % PAGE_SIZE == 0);
        assert(p_arena->current_offset <= p_arena->committed_size);
        assert(p_arena->committed_size <= p_arena->reserved_size);

        // Align the absolute address, not just the arena-relative offset, in
        // case the caller requests alignment stricter than the page size.
        // current_address cannot overflow, every offset up to reserved_size
        // lies within the reserved mapping, so the address is representable.
        const std::uintptr_t base_address = reinterpret_cast<std::uintptr_t>(p_arena->base_ptr);
        const std::uintptr_t current_address = base_address + p_arena->current_offset;
        const std::size_t padding = static_cast<std::size_t>(
                (std::uintptr_t{0} - current_address) & (alignment - 1)
                );

        // padding < alignment, and current_offset <= reserved_size, so this
        // addition cannot wrap unless alignment is absurd.
        // check via subtraction to be airtight.
        if(padding > p_arena->reserved_size - p_arena->current_offset) {
            return std::unexpected(alloc_error::NotEnoughMemory);
        }
        const std::size_t aligned_offset = p_arena->current_offset + padding;

        if(size > p_arena->reserved_size - aligned_offset) {
            return std::unexpected(alloc_error::NotEnoughMemory);
        }
        const std::size_t new_offset = aligned_offset + size;

        if(new_offset > p_arena->committed_size) {
            // Round up to the next page boundary. new_offset <= reserved_size
            // and reserved_size is page-aligned, so new_commit_target is too,
            // and this cannot overflow.
            const std::size_t new_commit_target = (new_offset + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
            const std::size_t size_to_commit = new_commit_target - p_arena->committed_size;
            std::byte* commit_start_addr = p_arena->base_ptr + p_arena->committed_size;

#ifdef _WIN32
        if(!VirtualAlloc(commit_start_addr, size_to_commit, MEM_COMMIT,
                    PAGE_READWRITE)) {
            return std::unexpected(alloc_error::CouldNotCommitMemory);
        }
#else
        if(mprotect(commit_start_addr, size_to_commit, PROT_READ | PROT_WRITE) != 0) {
            return std::unexpected(alloc_error::CouldNotCommitMemory);
        }
#endif
            p_arena->committed_size = new_commit_target;
        }

        std::byte* memory = p_arena->base_ptr + aligned_offset;
        p_arena->current_offset = new_offset;
        return memory;
    }

}
