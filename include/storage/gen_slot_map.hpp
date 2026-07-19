#pragma once

#include "transient_ptr.hpp"

#include <concepts>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace stdan::storage {
inline constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

template<typename T>
class generational_slot_map {
private:
    struct slot {
        union {
            T value;
            std::uint32_t nextFree;
        };

        std::uint32_t generation = 1;
        bool active = false;

        slot() noexcept
            : nextFree(0) {}

        slot(const slot& otherSlot)
            noexcept(std::is_nothrow_copy_constructible_v<T>)
            requires std::is_copy_constructible_v<T>
            : nextFree(0)
            , generation(otherSlot.generation)
            , active(false) {
                if(otherSlot.active) {
                    std::construct_at(
                            std::addressof(value),
                            otherSlot.value
                            );

                    active = true;
                } else {
                    nextFree = otherSlot.nextFree;
                }
            }

        slot(slot&& otherSlot)
            noexcept(std::is_nothrow_move_constructible_v<T>)
            requires std::is_move_constructible_v<T>
            : nextFree(0)
            , generation(otherSlot.generation)
            , active(false) {
                if(otherSlot.active) {
                    std::construct_at(std::addressof(value), std::move(otherSlot.value));
                    active = true;
                } else {
                    nextFree = otherSlot.nextFree;
                }
            }

        ~slot() {
            if(active) {
                std::destroy_at(std::addressof(value));
            }
        }
    };

    template<bool IsConst>
    class basic_iterator {
        using slot_iterator = std::conditional_t<
                IsConst,
                typename std::vector<slot>::const_iterator,
                typename std::vector<slot>::iterator>;

public:
    // Iterator contract stuff for library machinery like
    // `std::ranges::find` and `std::copy`.
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using reference = std::conditional_t<IsConst, const T&, T&>;
    using pointer = std::conditional_t<IsConst, const T*, T*>;

    basic_iterator() = default;

    template<bool OtherConst> requires (IsConst && !OtherConst)
    basic_iterator(const basic_iterator<OtherConst>& other) noexcept
        : current_(other.current_)
        , end_(other.end_) {}

    reference operator*() const noexcept { return current_->value; }
    pointer operator->() const noexcept { return std::addressof(current_->value); }

    basic_iterator& operator++() noexcept {
        ++current_;
        skip_inactive();
        return *this;
    }

    basic_iterator operator++(int) noexcept {
        basic_iterator previous = *this;
        ++(*this);
        return previous;
    }

    template<bool OtherConst>
    bool operator==(const basic_iterator<OtherConst>& other) const noexcept {
        return current_ == other.current_;
    }

private:
    friend class generational_slot_map;
    template<bool>
    friend class basic_iterator;

    basic_iterator(slot_iterator current, slot_iterator end) noexcept
        : current_(current)
        , end_(end) {
        skip_inactive();
    }

    void skip_inactive() noexcept {
        while(current_ != end_ && !current_->active) { ++current_; }
    }

    slot_iterator current_{};
    slot_iterator end_{};
};

public:
    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;

    struct key {
        std::uint32_t index;
        std::uint32_t generation;
    };

    std::uint32_t size() const { return size_; }
    iterator begin() noexcept { return iterator(slots_.begin(), slots_.end()); }
    const_iterator begin() const noexcept { return cbegin(); }
    const_iterator cbegin() const noexcept { return const_iterator(slots_.cbegin(), slots_.cend()); }
    iterator end() noexcept { return iterator(slots_.end(), slots_.end()); }
    const_iterator end() const noexcept { return cend(); }
    const_iterator cend() const noexcept { return const_iterator(slots_.cend(), slots_.cend()); }
    bool empty() const { return size_ == 0; }
    bool contains(key key) const { return get(key) != nullptr; }
    void clear() {
        slots_.clear();
        freeHead_ = InvalidIndex;
        size_ = 0;
    }
    std::size_t capacity() const { return slots_.capacity(); }
    void reserve(std::size_t amt) { return slots_.reserve(amt); }

    key insert(const T& new_value) requires std::is_copy_constructible_v<T> {
        if(freeHead_ != InvalidIndex) {
            const std::uint32_t index = freeHead_;
            slot& s = slots_[index];
            const std::uint32_t next = s.nextFree;

            if constexpr (!std::is_nothrow_copy_constructible_v<T>) {
                try {
                    std::construct_at(std::addressof(s.value), new_value);
                    freeHead_ = next;
                    s.active = true;
                    ++size_;
                    return {index, s.generation};
                } catch (...) {
                    s.nextFree = next;
                    freeHead_ = index;
                    throw;
                }
            } else {
                std::construct_at(std::addressof(s.value), new_value);
                freeHead_ = next;
                s.active = true;
                ++size_;
                return {index, s.generation};
            }
        }

        const std::uint32_t index = static_cast<std::uint32_t>(slots_.size());
        slots_.emplace_back();
        slot& s = slots_.back();

        if constexpr (!std::is_nothrow_copy_constructible_v<T>) {
            try {
                std::construct_at(std::addressof(s.value), new_value);
                s.active = true;
                ++size_;
                return {index, s.generation};
            } catch(...) {
                slots_.pop_back();
                throw;
            }
        } else {
            std::construct_at(std::addressof(s.value), new_value);
            s.active = true;
            ++size_;
            return {index, s.generation};
        }
    }

    key insert(T&& value) requires std::is_move_constructible_v<T> {
        if(freeHead_ != InvalidIndex) {
            const std::uint32_t index = freeHead_;
            slot& s = slots_[index];
            const std::uint32_t next = s.nextFree;

            if constexpr (!std::is_nothrow_move_constructible_v<T>) {
                try {
                    std::construct_at(std::addressof(s.value), std::move(value));
                    freeHead_ = next;
                    s.active = true;
                    ++size_;
                    return {index, s.generation};
                } catch (...) {
                    s.nextFree = next;
                    freeHead_ = index;
                    throw;
                }
            } else {
                std::construct_at(std::addressof(s.value), std::move(value));
                freeHead_ = next;
                s.active = true;
                ++size_;
                return {index, s.generation};
            }
        }

        const std::uint32_t index = static_cast<std::uint32_t>(slots_.size());
        slots_.emplace_back();
        slot& s = slots_.back();

        if constexpr (!std::is_nothrow_move_constructible_v<T>) {
            try {
                std::construct_at(std::addressof(s.value), std::move(value));
                s.active = true;
                ++size_;
                return {index, s.generation};
            } catch(...) {
                slots_.pop_back();
                throw;
            }
        } else {
            std::construct_at(std::addressof(s.value), std::move(value));
            s.active = true;
            ++size_;
            return {index, s.generation};
        }
    }

    template<typename... Args>
    key emplace(Args&&... args) requires std::constructible_from<T, Args...> {
        if(freeHead_ != InvalidIndex) {
            const std::uint32_t index = freeHead_;
            slot& s = slots_[index];
            const std::uint32_t next = s.nextFree;

            if constexpr (!std::is_nothrow_constructible_v<T, Args...>) {
                try {
                    std::construct_at(std::addressof(s.value), std::forward<Args>(args)...);
                    freeHead_ = next;
                    s.active = true;
                    ++size_;
                    return {index, s.generation};
                } catch (...) {
                    s.nextFree = next;
                    freeHead_ = index;
                    throw;
                }
            } else {
                std::construct_at(std::addressof(s.value), std::forward<Args>(args)...);
                freeHead_ = next;
                s.active = true;
                ++size_;
                return {index, s.generation};
            }
        }

        const std::uint32_t index = static_cast<std::uint32_t>(slots_.size());
        slots_.emplace_back();
        slot& s = slots_.back();

        if constexpr (!std::is_nothrow_constructible_v<T, Args...>) {
            try {
                std::construct_at(std::addressof(s.value), std::forward<Args>(args)...);
                s.active = true;
                ++size_;
                return {index, s.generation};
            } catch (...) {
                slots_.pop_back();
                throw;
            }
        } else {
            std::construct_at(std::addressof(s.value), std::forward<Args>(args)...);
            s.active = true;
            ++size_;
            return {index, s.generation};
        }
    }

    bool remove(key key) {
        if(key.index >= slots_.size()) { return false; }
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
        if(key.index >= slots_.size()) return nullptr;
        slot& s = slots_[key.index];
        if(!s.active || s.generation != key.generation) return nullptr;
        return transient_ptr<T>(&s.value);
    }

    transient_ptr<const T> get(key key) const {
        if(key.index >= slots_.size()) return nullptr;
        const slot& s = slots_[key.index];
        if(!s.active || s.generation != key.generation) return nullptr;
        return transient_ptr<const T>(&s.value);
    }

private:
    std::vector<slot> slots_;
    std::uint32_t freeHead_ = InvalidIndex;
    std::uint32_t size_ = 0;
};
}
