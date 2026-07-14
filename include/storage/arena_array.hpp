#pragma once

#include "memory/arena.hpp"
#include "memory/memory_base.hpp"

#include <cstddef>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace stdan::storage {
    /// Your basic array, but now with the "reset" option.
    /// Upon reset, all pointers into this arena will be considered invalid.
    template<typename T, std::size_t ElementCapacity>
    class arena_array {
    public:
        static_assert(
                ElementCapacity == 0 || sizeof(T) <= (std::numeric_limits<std::size_t>::max)() / ElementCapacity,
                "sizeof(T) * ElementCapacity overflows std::size_t"
        );
        /// Constructor requires the user to handle arena creation
        /// and passing the dependency in.
        /// Ownership of the arena is considered to be taken over
        /// *solely* by the arena_array.
        arena_array() {
            auto result = memory::create_arena(sizeof(T) * ElementCapacity);
            if(!result) {
                // I don't like throws, but a bad alloc is *truly*
                // an exceptional occasion.
                throw std::bad_alloc();
            }
            arena_ = std::move(result.value());
            if(!arena_) { capacity_ = 0; }
        }

        ~arena_array() {
            if(!arena_) {
                return;
            }
            if constexpr (!std::is_trivially_destructible_v<T>) {
                T* first = reinterpret_cast<T*>(arena_->base_ptr);
                std::destroy(first, first + size_);
            }
        }

        const std::size_t size()     const { return size_; }
        const std::size_t capacity() const { return capacity_; }

        arena_array(arena_array&& other)                 = delete;
        arena_array(const arena_array& other)            = delete;
        arena_array& operator=(const arena_array& other) = delete;

        void reset() {
            if(!arena_) { return; }
            if constexpr (!std::is_trivially_destructible_v<T>) {
                T* first = reinterpret_cast<T*>(arena_->base_ptr);
                std::destroy(first, first + size_);
            }
            stdan::memory::arena_reset(arena_.get());
            size_ = 0;
        }

        // Sequentially bump-allocate and constructs the next element.
        // Returns false if the arena is missing, capacity has been reached,
        // or construction/allocation fails.
        template<typename... Args> requires std::is_nothrow_constructible_v<T, Args...>
        bool emplace_back(Args&&... args) {
            if(!arena_ || size_ >= capacity_) { return false; }

            auto created = memory::arena_construct<T>(arena_.get(), std::forward<Args>(args)...);
            if(!created) { return false; }
            ++size_;
            return true;
        }

        std::optional<std::reference_wrapper<T>> get(std::size_t idx) {
            if(!arena_ || idx >= size_) { return {}; }
            auto ret = __get_offsets(idx);
            if(!ret) { return {}; }
            auto [target, _] = ret.value();
            if(target >= arena_->current_offset) { return {}; }
            return std::optional(std::ref(*reinterpret_cast<T*>(arena_->base_ptr + target)));
        }

        std::optional<std::reference_wrapper<const T>> get(std::size_t idx) const {
            if(!arena_ || idx >= size_) { return {}; }
            auto ret = __get_offsets(idx);
            if(!ret) { return {}; }
            auto [target, _] = ret.value();
            if(target >= arena_->current_offset) { return {}; }
            return std::optional(std::cref(*reinterpret_cast<const T*>(arena_->base_ptr + target)));
        }

        template<typename Func> requires std::is_invocable_v<Func, T*>
        void apply(Func&& f) {
            for(std::size_t i = 0; i < size_; ++i) {
                T* t = reinterpret_cast<T*>(arena_->base_ptr + (i * sizeof(T)));
                f(t);
            }
        }

    private:
        const inline std::expected<std::tuple<std::size_t, std::size_t>, memory::alloc_error> __get_offsets(std::size_t idx) const {
            const std::size_t target_offset = idx * sizeof(T);
            const std::size_t target_end = target_offset + sizeof(T);
            if(target_end < target_offset || target_end > arena_->reserved_size) {
                return std::unexpected(memory::alloc_error::OutOfBounds);
            }
            return std::make_tuple(target_offset, target_end);
        }

        memory::arena_owner arena_ = nullptr;
        std::size_t capacity_ = ElementCapacity;
        std::size_t size_ = 0;
    };
}
