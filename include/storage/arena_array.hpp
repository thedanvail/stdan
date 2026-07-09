#pragma once

#include "memory/arena.hpp"
#include "memory/memory_base.hpp"

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
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
        /// Constructor requires the user to handle arena creation
        /// and passing the dependency in.
        /// Ownership of the arena is considered to be taken over
        /// *solely* by the arena_array.
        arena_array(memory::arena* p_arena) {
            if(p_arena == nullptr) { 
                capacity_ = 0;
                return; 
            }
            arena_.reset(p_arena);
        }
    
        arena_array(std::unique_ptr<memory::arena> ptr) { 
            if(ptr == nullptr) { capacity_ = 0; }
            arena_ = std::move(ptr); 
        }
    
        ~arena_array() {
            if(!arena_) {
                return;
            }
            if constexpr (!std::is_trivially_destructible_v<T>) {
                T* first = reinterpret_cast<T*>(arena_->base_ptr);
                std::destroy(first, first + size_);
            }
            auto ptr = arena_.release();
            memory::arena_release(ptr);
        }


        std::size_t size()     const { return size_; }
        std::size_t capacity() const { return capacity_; }

        arena_array(arena_array&& other)                 = delete;
        arena_array(const arena_array& other)            = delete;
        arena_array& operator=(const arena_array& other) = delete;
    
        void reset() {
            if(!arena_) { return; }
            stdan::memory::arena_reset(arena_.get());
            size_ = 0;
        }
    
        // If the target slot is already below the current bump pointer, we
        // reconstruct in-place at that exact location. If the slot extends past
        // the bump pointer, we allocate the gap up to and including that slot.
        void emplace_back(T&& element) {
            if(!arena_ || size_ >= capacity_) { return; }

            auto created = memory::arena_construct<T>(arena_.get(), std::move(element));
            if(!created) { return; }
            ++size_;
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
        inline std::expected<std::tuple<std::size_t, std::size_t>, memory::alloc_error> __get_offsets(std::size_t idx) {
            const std::size_t target_offset = idx * sizeof(T);
            const std::size_t target_end = target_offset + sizeof(T);
            if(target_end < target_offset || target_end > arena_->reserved_size) {
                return std::unexpected(memory::alloc_error::OutOfBounds);
            }
            return std::make_tuple(target_offset, target_end);
        }
    
        std::unique_ptr<memory::arena> arena_ = nullptr;
        std::size_t capacity_ = ElementCapacity;
        std::size_t size_ = 0;
    };
}
