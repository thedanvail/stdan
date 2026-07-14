#pragma once

#include "storage/storage_base.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <execution>
#include <expected>
#include <type_traits>
#include <utility>
#include <vector>

namespace stdan::storage {

/// A pop-swap vector.
/// Saves a lot of space/speed by omitting the actual deletion of T
/// instances and only considering up to the last valid instance
/// for any processing.
/// As a tradeoff, the underlying vector will often be mutated,
/// so getting a reference or pointer to the underlying object
/// is very dangerous as you'll have to be judicious with your
/// use of it during its appropriate lifetime; for this reason,
/// it's my opinion that you should simply chain a `get` call or use
/// it in-place instead of holding onto it as an l-value. Treat it as
/// a short-lived x-value for safety purposes.
/// Your other solid option is to filter out any elements you do not
/// need and then run an operation on that.
template<typename T> requires std::is_default_constructible_v<T> && std::is_move_constructible_v<T>
class ps_vector {
public:
    // if your T is hella expensive to construct, this is perfect;
    // however, if it's trivially constructible, I'd recomment you call
    // `resize` immediately after constructing a ps_vector so
    // that the construction can all take place at once.
    // You live your life tho
    ps_vector(std::size_t aCapacity)
        : m_capacity(aCapacity)
        , m_firstAvailableEntry(0) {
        data_.reserve(aCapacity);
    }

    ~ps_vector()                                  = default;
    ps_vector(ps_vector&& aOther)                 = default;
    ps_vector(const ps_vector& aOther)            = default;
    ps_vector& operator=(ps_vector&& aOther)      = default;
    ps_vector& operator=(const ps_vector& aOther) = default;

private:
    std::vector<T> data_;
    std::size_t m_capacity;
    std::size_t m_firstAvailableEntry;

public:

    // Iterator stuff for loop support
    auto begin() noexcept { return data_.begin(); }
    auto begin() const noexcept { return data_.cbegin(); }
    auto cbegin() const noexcept { return data_.cbegin(); }
    auto end() noexcept { return data_.begin() + m_firstAvailableEntry; }
    auto end() const noexcept { return data_.cbegin() + m_firstAvailableEntry; }
    auto cend() const noexcept { return data_.cbegin() + m_firstAvailableEntry; }

    bool empty() const { return m_firstAvailableEntry == 0; }
    bool full() const { return m_firstAvailableEntry == m_capacity; }
    std::size_t size() const { return m_firstAvailableEntry; }
    std::size_t capacity() const { return m_capacity; }
    std::size_t first_available() const { return m_firstAvailableEntry; }

    [[nodiscard]] T& operator[](std::size_t idx) {
#ifdef STDAN_DEBUG
        assert(idx < m_firstAvailableEntry);
#endif
        return data_[idx];
    }

    [[nodiscard]] const T& operator[](std::size_t idx) const {
#ifdef STDAN_DEBUG
        assert(idx < m_firstAvailableEntry);
#endif
        return data_[idx];
    }

    void resize(std::size_t aNewSize) requires std::default_initializable<T> {
        data_.resize(aNewSize);
        m_firstAvailableEntry = aNewSize;
    }

    [[nodiscard]] std::expected<std::size_t, error_code> index_of(const T& t) const requires std::equality_comparable<T> {
        auto it = std::find(data_.begin(), data_.begin() + m_firstAvailableEntry, t);
        if(it != data_.begin() + m_firstAvailableEntry) {
            return static_cast<std::size_t>(std::distance(data_.begin(), it));
        }
        return std::unexpected(error_code::ItemNotFound);
    }

    void append(T&& t) {
        if(full()) [[unlikely]] { return; }

        if (m_firstAvailableEntry < data_.size()) [[likely]] { data_[m_firstAvailableEntry] = std::move(t); }
        else { data_.emplace_back(std::move(t)); }
        ++m_firstAvailableEntry;
    }

    void append(const T& t) requires std::is_copy_constructible_v<T> {
        if(full()) [[unlikely]] { return; }

        if(m_firstAvailableEntry < data_.size()) { data_[m_firstAvailableEntry] = t; }
        else { data_.emplace_back(t); }
        ++m_firstAvailableEntry;
    }

    /// Considers the removed item as dead and gone but does not run the destructor.
    /// If you need to run the destructor, call `destroy`.
    void remove(std::size_t idx) {
#ifdef STDAN_DEBUG
        assert(idx < m_firstAvailableEntry);
#endif
        if(idx >= m_firstAvailableEntry) [[unlikely]] { return; }

        --m_firstAvailableEntry;
        if(idx != m_firstAvailableEntry) {
            // Use move assignment instead of swap to save 2 operations
            data_[idx] = std::move(data_[m_firstAvailableEntry]);
        }
    }

    /// The same as `remove` except also runs the destructor.
    void destroy(std::size_t idx) requires std::is_nothrow_destructible_v<T> && std::is_nothrow_move_constructible_v<T> {
        if(idx >= m_firstAvailableEntry) [[unlikely]] { return; }
        if(idx != --m_firstAvailableEntry) {
            std::destroy_at(std::addressof(data_[idx]));
            std::construct_at(std::addressof(data_[idx]), std::move(data_[m_firstAvailableEntry]));
        }
    }

    /// Retrieves the const pointer for the element at the index.
    /// NOTE: Be careful about using this. It is heavily prone to misuse
    /// due to the fact that elements may pop/swap/be moved and the underlying
    /// pointer will then become invalid. 
    /// In fact, unless you are 100% sure you know what you're doing (and you probably don't),
    /// don't use this. Prefer to edit the item in-place.
    [[nodiscard]] std::expected<const T*, error_code> get(std::size_t idx) const {
        if(idx >= m_firstAvailableEntry) { return std::unexpected(error_code::IndexOutOfBounds); }
        return &data_[idx];
    }

    /// Retrieves the pointer to a mutable reference for the element at the index.
    /// NOTE: Be careful about using this. It is heavily prone to misuse
    /// due to the fact that elements may pop/swap/be moved and the underlying
    /// pointer will then become invalid. 
    /// In fact, unless you are 100% sure you know what you're doing (and you probably don't),
    /// don't use this. Prefer to edit the item in-place.
    [[nodiscard]] std::expected<T*, error_code> get(std::size_t idx) {
        if(idx >= m_firstAvailableEntry) { return std::unexpected(error_code::IndexOutOfBounds); }
        return &data_[idx];
    }

    // We need one for const ps_vectors and one for non-const.
    template <typename ExecutionPolicy, typename F>
        requires std::is_execution_policy_v<std::remove_cvref_t<ExecutionPolicy>> && std::is_invocable_v<F&, T&>
    void for_each(ExecutionPolicy&& policy, F&& aFunc) {
        std::for_each(
             std::forward<ExecutionPolicy>(policy),
              data_.begin(),
              data_.begin() + static_cast<std::ptrdiff_t>(m_firstAvailableEntry),
              [&aFunc](T& data) { aFunc(data); }
        );
    }

    // The const version
    template <typename ExecutionPolicy, typename F>
        requires std::is_execution_policy_v<std::remove_cvref_t<ExecutionPolicy>> && std::is_invocable_v<F&, const T&>
    void for_each(ExecutionPolicy&& policy, F&& aFunc) const {
        std::for_each(
             std::forward<ExecutionPolicy>(policy),
              data_.cbegin(),
              data_.cbegin() + static_cast<std::ptrdiff_t>(m_firstAvailableEntry),
              [&aFunc](const T& data) { aFunc(data); }
        );
    }

    /* Convenience functions because we do all be lazy sometimes */

    /// Uses multiple threads to process elements simultaneously. 
    /// Use this when the work done inside the function is heavy.
    /// 
    /// ensure your function is thread-safe. Multiple threads 
    /// will access different elements, but if they access shared global data, 
    /// you must use mutexes or atomics. or don't. I'm not your father.
    template<typename F> requires std::is_invocable_v<F&, T&>
    void for_each_par(F&& aFunc) {
        for_each(std::execution::par, std::forward<F>(aFunc));
    }

    template<typename F> requires std::is_invocable_v<F&, const T&>
    void for_each_par(F&& aFunc) const {
        for_each(std::execution::par, std::forward<F>(aFunc));
    }

    /// This is the standard, predictable behavior. It runs on the current 
    /// thread in order from the first element to the last.
    /// 
    /// Note: best used for small datasets or functions that are very fast.
    template<typename F> requires std::is_invocable_v<F&, T&>
    void for_each_seq(F&& aFunc) {
        for_each(std::execution::seq, std::forward<F>(aFunc));
    }

    template<typename F> requires std::is_invocable_v<F&, const T&>
    void for_each_seq(F&& aFunc) const {
        for_each(std::execution::seq, std::forward<F>(aFunc));
    }

    /// Runs on a single thread but allows the compiler to use special CPU 
    /// instructions to process multiple elements at once (instruction-level parallelism).
    /// 
    /// aFunc must not perform synchronization (like locking a mutex), 
    /// as this can cause deadlocks in unsequenced mode.
    template<typename F> requires std::is_invocable_v<F&, T&>
    void for_each_unseq(F&& aFunc) {
        for_each(std::execution::unseq, std::forward<F>(aFunc));
    }

    template<typename F> requires std::is_invocable_v<F&, const T&>
    void for_each_unseq(F&& aFunc) const {
        for_each(std::execution::unseq, std::forward<F>(aFunc));
    }

    /// Spreads work across multiple threads AND uses SIMD instructions. 
    /// This offers the highest potential performance for massive datasets.
    /// 
    /// Combines the risks of Parallel and Unsequenced: must be 
    /// thread-safe and must not use synchronization primitives.
    template<typename F> requires std::is_invocable_v<F&, T&>
    void for_each_par_unseq(F&& aFunc) {
        for_each(std::execution::par_unseq, std::forward<F>(aFunc));
    }

    template<typename F> requires std::is_invocable_v<F&, const T&>
    void for_each_par_unseq(F&& aFunc) const {
        for_each(std::execution::par_unseq, std::forward<F>(aFunc));
    }

    // Default to sequential for safety
    template<typename F> requires std::is_invocable_v<F&, T&>
    void for_each(F&& aFunc) {
        for_each(std::execution::seq, std::forward<F>(aFunc));
    }

    template<typename F> requires std::is_invocable_v<F&, const T&>
    void for_each(F&& aFunc) const {
        for_each(std::execution::seq, std::forward<F>(aFunc));
    }
};

}
