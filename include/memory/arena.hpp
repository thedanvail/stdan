#pragma once

#include "memory_base.hpp"

#include <cstddef>
#include <expected>
#include <new> // IWYU pragma: keep
#include <utility>

namespace stdan::memory
{
    /// Your basic arena allocator - primarily designed for trivially-constructible types.
    /// You are responsible for any clean-up of non-trivially destructible types
    /// and you should be aware of the use cases/limitations of this - specifically,
    /// in loops (like a game engine update loop) and when you should/shouldn't use this.
    /// NOTE: It is not thread-safe. Maybe another time.
    struct arena
    {
        std::byte*  base_ptr;
        std::size_t reserved_size;
        std::size_t committed_size;
        std::size_t current_offset;
    };

    [[nodiscard]] std::expected<arena*,     alloc_error> create_arena(std::size_t reserve_size);
    [[nodiscard]] std::expected<std::byte*, alloc_error> arena_alloc(arena* p_arena, std::size_t size, std::size_t alignment = alignof(std::max_align_t));
    /// Resets the offset pointer. Note that it does NOT deallocate the reserved physical memory.
    void arena_reset(arena* p_arena);

    /// Deallocates the memory. Any destructors should be run by the function that allocated
    /// the memory.
    void arena_release(arena* p_arena);

    template<typename T, typename... Args>
    [[nodiscard]] inline std::expected<T*, alloc_error> arena_construct(arena* a, Args&&... args)
    {
        auto raw = arena_alloc(a, sizeof(T), alignof(T));
        if(!raw) { return std::unexpected(raw.error()); }
        // placement new babyyyyyy
        return ::new (*raw) T(std::forward<Args>(args)...);
    }
}

