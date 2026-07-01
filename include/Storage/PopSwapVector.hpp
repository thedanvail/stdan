#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <execution>
#include <expected>
#include <type_traits>
#include <utility>
#include <vector>

template <typename T>
concept ValidType = std::is_default_constructible_v<T> && std::is_move_constructible_v<T>;

template<ValidType T>
class PopSwapVector
{
public:
    // if your T is hella expensive to construct, this is perfect;
    // however, if it's trivially constructible, I'd recomment you call
    // `Resize` immediately after constructing a PopSwapVector so
    // that the construction can all take place at once.
    // You live your life tho
    PopSwapVector(std::size_t aCapacity)
        : m_capacity(aCapacity)
        , m_firstAvailableEntry(0)
    {
        m_data.reserve(aCapacity);
    }

    ~PopSwapVector()                                       = default;
    PopSwapVector(PopSwapVector&& aOther)                  = default;
    PopSwapVector(const PopSwapVector& aOther)             = default;
    PopSwapVector& operator=(PopSwapVector&& aOther)       = default;
    PopSwapVector& operator=(const PopSwapVector& aOther)  = default;

    enum class ErrorCode
    {
        ITEM_NOT_FOUND
    };

private:
    std::vector<T> m_data;
    std::size_t m_capacity;
    std::size_t m_firstAvailableEntry;

public:

    // Iterator stuff for loop support
    auto begin() noexcept { return m_data.begin(); }
    auto begin() const noexcept { return m_data.cbegin(); }
    auto cbegin() const noexcept { return m_data.cbegin(); }
    auto end() noexcept { return m_data.begin() + m_firstAvailableEntry; }
    auto end() const noexcept { return m_data.cbegin() + m_firstAvailableEntry; }
    auto cend() const noexcept { return m_data.cbegin() + m_firstAvailableEntry; }

    bool IsEmpty() const { return m_firstAvailableEntry == 0; }
    bool IsFull() const { return m_firstAvailableEntry == m_capacity; }
    std::size_t Size() const { return m_firstAvailableEntry; }
    std::size_t Capacity() const { return m_capacity; }
    std::size_t FirstAvailableEntry() const { return m_firstAvailableEntry; }

    [[nodiscard]] T& operator[](std::size_t idx)
    {
#ifdef STDAN_DEBUG
        assert(idx < m_firstAvailableEntry);
#endif
        return m_data[idx];
    }

    [[nodiscard]] const T& operator[](std::size_t idx) const
    {
#ifdef STDAN_DEBUG
        assert(idx < m_firstAvailableEntry);
#endif
        return m_data[idx];
    }

    void Resize(std::size_t aNewSize) requires std::default_initializable<T>
    {
        if(aNewSize > m_capacity) [[unlikely]] { return; }

        if(aNewSize > m_data.size())
        {
            // Grow the physical storage to accommodate the new logical size
            m_data.resize(aNewSize);
        }

        m_firstAvailableEntry = aNewSize;
    }

    [[nodiscard]] std::expected<std::size_t, ErrorCode> IndexOf(const T& t) const requires std::equality_comparable<T>
    {
        auto it = std::find(m_data.begin(), m_data.begin() + m_firstAvailableEntry, t);
        if (it != m_data.begin() + m_firstAvailableEntry) {
            return static_cast<std::size_t>(std::distance(m_data.begin(), it));
        }
        return std::unexpected(ErrorCode::ITEM_NOT_FOUND);
    }

    void Append(T&& t)
    {
        if (IsFull()) [[unlikely]] { return; }

        if (m_firstAvailableEntry < m_data.size()) [[likely]] { m_data[m_firstAvailableEntry] = std::move(t); }
        else { m_data.emplace_back(std::move(t)); }
        ++m_firstAvailableEntry;
    }

    void Append(const T& t) requires std::is_copy_constructible_v<T>
    {
        if(IsFull()) { return; }

        if(m_firstAvailableEntry < m_data.size()) { m_data[m_firstAvailableEntry] = t; }
        else { m_data.emplace_back(t); }
        ++m_firstAvailableEntry;
        return;
    }

    void Remove(std::size_t idx)
    {
#ifdef STDAN_DEBUG
        assert(idx < m_firstAvailableEntry);
#endif
        if(idx >= m_firstAvailableEntry) [[unlikely]] { return; }

        --m_firstAvailableEntry;
        if(idx != m_firstAvailableEntry)
        {
            // Use move assignment instead of swap to save 2 operations
            m_data[idx] = std::move(m_data[m_firstAvailableEntry]);
        }
    }

    /// Retrieves the const pointer for the element at the index.
    /// NOTE: If index is invalid, pointer will be nullptr.
    /// NOTE: Be careful about using this. It is heavily prone to misuse
    /// due to the fact that elements may pop/swap/be moved and the underlying
    /// pointer will then become invalid. 
    /// In fact, unless you are 100% sure you know what you're doing (and you probably don't),
    /// don't use this.
    [[nodiscard]] std::expected<const T*, ErrorCode> Get(std::size_t idx) const
    {
        if(idx >= m_firstAvailableEntry) { return std::unexpected(ErrorCode::ITEM_NOT_FOUND); }
        return &m_data[idx];
    }

    /// Retrieves the pointer to a mutable reference for the element at the index.
    /// NOTE: If index is invalid, pointer will be nullptr.
    /// NOTE: Be careful about using this. It is heavily prone to misuse
    /// due to the fact that elements may pop/swap/be moved and the underlying
    /// pointer will then become invalid. 
    /// In fact, unless you are 100% sure you know what you're doing (and you probably don't),
    /// don't use this.
    [[nodiscard]] std::expected<T*, ErrorCode> Get(std::size_t idx)
    {
        if(idx >= m_firstAvailableEntry) { return std::unexpected(ErrorCode::ITEM_NOT_FOUND); }
        return &m_data[idx];
    }

    // We need one for const PopSwapVectors and one for non-const.
    template <typename ExecutionPolicy, typename F>
        requires std::is_execution_policy_v<std::remove_cvref_t<ExecutionPolicy>> && std::is_invocable_v<F&, T&>
    void ForEach(ExecutionPolicy&& policy, F&& aFunc)
    {
        std::for_each(
             std::forward<ExecutionPolicy>(policy),
              m_data.begin(),
              m_data.begin() + static_cast<std::ptrdiff_t>(m_firstAvailableEntry),
              [&aFunc](T& data) { aFunc(data); }
        );
    }

    // The const version
    template <typename ExecutionPolicy, typename F>
        requires std::is_execution_policy_v<std::remove_cvref_t<ExecutionPolicy>> && std::is_invocable_v<F&, const T&>
    void ForEach(ExecutionPolicy&& policy, F&& aFunc) const
    {
        std::for_each(
             std::forward<ExecutionPolicy>(policy),
              m_data.cbegin(),
              m_data.cbegin() + static_cast<std::ptrdiff_t>(m_firstAvailableEntry),
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
    void ForEachParallel(F&& aFunc)
    {
        ForEach(std::execution::par, std::forward<F>(aFunc));
    }

    template<typename F> requires std::is_invocable_v<F&, const T&>
    void ForEachParallel(F&& aFunc) const
    {
        ForEach(std::execution::par, std::forward<F>(aFunc));
    }

    /// This is the standard, predictable behavior. It runs on the current 
    /// thread in order from the first element to the last.
    /// 
    /// Note: best used for small datasets or functions that are very fast.
    template<typename F> requires std::is_invocable_v<F&, T&>
    void ForEachSequential(F&& aFunc)
    {
        ForEach(std::execution::seq, std::forward<F>(aFunc));
    }

    template<typename F> requires std::is_invocable_v<F&, const T&>
    void ForEachSequential(F&& aFunc) const
    {
        ForEach(std::execution::seq, std::forward<F>(aFunc));
    }

    /// Runs on a single thread but allows the compiler to use special CPU 
    /// instructions to process multiple elements at once (instruction-level parallelism).
    /// 
    /// aFunc must not perform synchronization (like locking a mutex), 
    /// as this can cause deadlocks in unsequenced mode.
    template<typename F> requires std::is_invocable_v<F&, T&>
    void ForEachUnsequenced(F&& aFunc)
    {
        ForEach(std::execution::unseq, std::forward<F>(aFunc));
    }

    template<typename F> requires std::is_invocable_v<F&, const T&>
    void ForEachUnsequenced(F&& aFunc) const
    {
        ForEach(std::execution::unseq, std::forward<F>(aFunc));
    }

    /// Spreads work across multiple threads AND uses SIMD instructions. 
    /// This offers the highest potential performance for massive datasets.
    /// 
    /// Combines the risks of Parallel and Unsequenced: must be 
    /// thread-safe and must not use synchronization primitives.
    template<typename F> requires std::is_invocable_v<F&, T&>
    void ForEachParallelUnsequenced(F&& aFunc)
    {
        ForEach(std::execution::par_unseq, std::forward<F>(aFunc));
    }

    template<typename F> requires std::is_invocable_v<F&, const T&>
    void ForEachParallelUnsequenced(F&& aFunc) const
    {
        ForEach(std::execution::par_unseq, std::forward<F>(aFunc));
    }

    // Default to sequential for safety
    template<typename F> requires std::is_invocable_v<F&, T&>
    void ForEach(F&& aFunc)
    {
        ForEach(std::execution::seq, std::forward<F>(aFunc));
    }

    template<typename F> requires std::is_invocable_v<F&, const T&>
    void ForEach(F&& aFunc) const
    {
        ForEach(std::execution::seq, std::forward<F>(aFunc));
    }
};
