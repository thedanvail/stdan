#pragma once

#include "transient_ptr.hpp"

#include <cstdint>
#include <type_traits>
#include <vector>

namespace stdan::storage {

template<typename T>
concept valid_type = std::is_destructible_v<T>
                     && std::is_nothrow_move_constructible_v<T>
                     || std::is_copy_constructible_v<T>;

template<valid_type T>
class generational_slot_map {
public:

    struct key {
        std::uint32_t index;
        std::uint32_t generation;
    };

    std::uint32_t size() const { return size_; }
    auto begin() noexcept { return slots_.begin(); }
    auto begin() const noexcept { return slots_.cbegin(); }
    auto cbegin() const noexcept { return slots_.cbegin(); }
    auto end() noexcept { return slots_.end(); }
    auto end() const noexcept { return slots_.cend(); }
    auto cend() const noexcept { return slots_.cend(); }
    bool empty() { return size_ == 0; }
    bool contains(key key) { return get(key) != nullptr; }
    void clear() { slots_.clear(); }
    std::size_t capacity() const { return slots_.capacity(); }
    void reserve(std::size_t amt) { return slots_.reserve(amt); }

    key insert(const T& value) {
        std::uint32_t index = __gen_index();
        slot& s = slots_[index];
        new (&s.value) T(value);
        s.active = true;
        ++size_;
        return {index, s.generation};
    }

    // TODO: Placeholder for variadic arg emplace
    //
    // template<typename... Args>
    // key emplace() requires std::constructible_from<T, Args>::value {};

    key emplace_back(T&& value) {
        std::uint32_t index = __gen_index();
        slot& s = slots_[index];
        new (&s.value) T(std::move(value));
        s.active = true;
        ++size_;
        return {index, s.generation};
    }

    bool remove(key key) {
        if (key.index >= slots_.size()) { return false; }
        slot& s = slots_[key.index];
        if(!s.active || s.generation != key.generation) { return false; }
        std::destroy_at(std::addressof(s.value));
        s.active = false;
        ++s.generation;
        s.nextFree = freeHead_;
        freeHead_ = key.index;
        --size_;
        return true;
    }

    transient_ptr<T> get(key key) {
        if (key.index >= slots_.size()) return nullptr;
        slot& s = slots_[key.index];
        if (!s.active || s.generation != key.generation) return nullptr;
        return transient_ptr<T>(&s.value);
    }

    transient_ptr<const T> get(key key) const {
        if (key.index >= slots_.size()) return nullptr;
        slot& s = slots_[key.index];
        if (!s.active || s.generation != key.generation) return nullptr;
        return transient_ptr<const T>(&s.value);
    }

private:
    struct slot {
        union {
            T value;
            std::uint32_t nextFree;
        };
        std::uint32_t generation = 1;
        bool active = false;

        slot() : nextFree(0) {}
        slot(slot&& otherSlot) noexcept
            : generation(otherSlot.generation)
            , active(otherSlot.active) {
            if(active) {
                std::construct_at(std::addressof(value), std::move(otherSlot.value));
                otherSlot.active = false;
            } else {
                nextFree = otherSlot.nextFree;
            }
        }
        ~slot() {
            if(active) { std::destroy_at(std::addressof(value)); }
        }
    };

    class basic_iterator {

    };

    std::uint32_t __gen_index() {
        std::uint32_t index;
        if(freeHead_ != UINT32_MAX) {
            index = freeHead_;
            freeHead_ = slots_[index].nextFree;
        } else {
            index = static_cast<std::uint32_t>(slots_.size());
            slots_.emplace_back();
        }
        return index;
    }

    std::vector<slot> slots_;
    std::uint32_t freeHead_ = UINT32_MAX;
    std::uint32_t size_ = 0;
};
}
