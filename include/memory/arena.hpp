#pragma once

#include "memory/memory_base.hpp"

#include <cstddef>
#include <expected>
#include <memory>
#include <new> // IWYU pragma: keep
#include <type_traits>
#include <utility>

namespace stdan::memory {
    /// Your basic arena allocator - primarily designed for trivially-constructible/destructible types.
    /// You are responsible for any clean-up of non-trivially destructible types
    /// and you should be aware of the use cases/limitations of this - specifically,
    /// in loops (like a game engine update loop) and when you should/shouldn't use this.
    /// NOTE: It is not thread-safe. Maybe another time.
    struct arena {
        std::byte*  base_ptr;
        std::size_t reserved_size;
        std::size_t committed_size;
        std::size_t current_offset;
    };

    struct arena_deleter {
        /// Performs best-effort release. Destructors cannot report an OS release failure
        /// call arena_release(owner.release()) explicitly when the status is needed.
        void operator()(arena* p_arena) const noexcept;
    };

    /// Move-only ownership of an arena.
    using arena_owner = std::unique_ptr<arena, arena_deleter>;

    /// Releases an arena. A successful call invalidates `p_arena`
    /// on failure it remains allocated so the caller can inspect
    /// the error or retry. A null pointer is a no-op.
    std::expected<void, alloc_error> arena_release(arena* p_arena) noexcept;

    [[nodiscard]] std::expected<arena_owner, alloc_error> create_arena(std::size_t reserve_size);
    [[nodiscard]] std::expected<std::byte*, alloc_error>  arena_alloc(arena* p_arena, std::size_t size, std::size_t alignment = alignof(std::max_align_t));

    /// Resets the offset pointer without ending object lifetimes or decommitting pages.
    /// Destroy every live non-trivially-destructible object before calling this function.
    void arena_reset(arena* p_arena) noexcept;

    template<typename T, typename... Args> requires std::is_nothrow_constructible_v<T, Args...>
    [[nodiscard]] inline std::expected<T*, alloc_error> arena_construct(arena* a, Args&&... args) {
        auto raw = arena_alloc(a, sizeof(T), alignof(T));
        if(!raw) { return std::unexpected(raw.error()); }
        // placement new babyyyyyy
        return ::new (*raw) T(std::forward<Args>(args)...);
    }

}
