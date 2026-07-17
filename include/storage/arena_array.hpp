#pragma once

#include "memory/arena.hpp"
#include "storage/transient_ptr.hpp"

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <optional>
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
        static constexpr std::size_t storage_size_ = sizeof(T) * ElementCapacity;
        static_assert(
                ElementCapacity == 0 || storage_size_ <= (std::numeric_limits<std::size_t>::max)() - (alignof(T) - 1),
                "arena_array storage plus alignment padding overflows std::size_t"
        );

        arena_array() {
            if constexpr (ElementCapacity == 0) { return; }

            auto result = memory::create_arena(storage_size_ + alignof(T) - 1);
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
                if(first_) { std::destroy(first_, first_ + size_); }
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
                if(first_) { std::destroy(first_, first_ + size_); }
            }
            stdan::memory::arena_reset(arena_.get());
            first_ = nullptr;
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
            if(size_ == 0) { first_ = created.value(); }
            ++size_;
            return true;
        }

        transient_ptr<T> get(std::size_t idx) {
            if(!first_ || idx >= size_) { return {}; }
            return transient_ptr<T>(&first_[idx]);
        }

        transient_ptr<T> get(std::size_t idx) const {
            if(!first_ || idx >= size_) { return {}; }
            return transient_ptr<T>(&first_[idx]);
        }

        template<typename Func> requires std::is_invocable_v<Func, T*>
        void apply(Func&& f) {
            for(std::size_t i = 0; i < size_; ++i) {
                f(first_ + i);
            }
        }

    private:
        memory::arena_owner arena_ = nullptr;
        T* first_ = nullptr;
        std::size_t capacity_ = ElementCapacity;
        std::size_t size_ = 0;
    };
}
