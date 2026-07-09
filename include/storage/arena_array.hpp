#pragma once

#include "arena.hpp"
#include "memory_base.hpp"

#include <cstddef>
#include <expected>
#include <memory>
#include <tuple>
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
            arena_ = std::move(ptr); 
            if(ptr == nullptr) { capacity_ = 0; }
        }
    
        ~arena_array() {
            auto ptr = arena_.release();
            memory::arena_release(ptr);
        }
        arena_array(const arena_array& other)            = delete;
        arena_array(const arena_array&& other)           = delete;
        arena_array& operator=(const arena_array& other) = delete;
    
        void reset() { stdan::memory::arena_reset(arena_.get()); }
    
        // If the target slot is already below the current bump pointer, we
        // reconstruct in-place at that exact location. If the slot extends past
        // the bump pointer, we allocate the gap up to and including that slot.
        template<typename... Args>
        std::expected<T*, memory::alloc_error> construct_at(std::size_t idx, Args&&... args) {
            if(idx >= capacity_) { return std::unexpected(memory::alloc_error::OutOfBounds); }
            auto [target_offset, target_end] = __get_offsets(idx);
    
            if(target_end > arena_->current_offset) {
                auto extend = memory::arena_alloc(arena_.get(), target_end - arena_->current_offset, alignof(T));
                if(!extend) { return std::unexpected(extend.error()); }
            }
    
            return ::new (target_offset) T(std::forward<Args>(args)...);
        }
    
        std::size_t capacity() const { return capacity_; }
    
        std::expected<T&, memory::alloc_error> get(std::size_t idx) {
            if(idx >= capacity_) { return memory::alloc_error::OutOfBounds; }
            auto [offset, end] = __get_offsets(idx);
            return *reinterpret_cast<T*>(arena_->base_ptr + offset);
        }
    
        std::expected<const T&, memory::alloc_error> get(std::size_t idx) const {
            auto ret = get(idx);
            if(!ret) { return std::unexpected(memory::alloc_error::OutOfBounds); }
            return const_cast<const T&>(ret.value());
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
    };
}
