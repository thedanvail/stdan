#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <execution>
#include <type_traits>
#include <utility>
#include <vector>

template<typename T>
class PopSwapVector
{
public:
    PopSwapVector(std::size_t aCapacity)
        : m_capacity(aCapacity)
    {
        m_data.reserve(aCapacity);
    }

    ~PopSwapVector()                                       = default;
    PopSwapVector(PopSwapVector&& aOther)                  = default;
    PopSwapVector(const PopSwapVector& aOther)             = default;
    PopSwapVector& operator=(PopSwapVector&& aOther)       = default;
    PopSwapVector& operator=(const PopSwapVector& aOther)  = default;

private:
    std::vector<T> m_data;
    std::size_t m_capacity;

public:
    T& operator[](std::size_t idx)
    {
#if defined(DAN_DEBUG) && DAN_DEBUG
        assert(idx < m_data.size());
#endif
        return m_data[idx];
    }

    const T& operator[](std::size_t idx) const
    {
#if defined(DAN_DEBUG) && DAN_DEBUG
        assert(idx < m_data.size());
#endif
        return m_data[idx];
    }

    void Resize(std::size_t aNewSize)
    {
        if(aNewSize > m_capacity) { return; }

        const std::size_t currentSize = m_data.size();
        if(aNewSize <= currentSize)
        {
            m_data.resize(aNewSize);
            return;
        }

        if constexpr(std::default_initializable<T>)
        {
            m_data.resize(aNewSize);
        }
        else
        {
#if defined(DAN_DEBUG) && DAN_DEBUG
            assert(false && "PopSwapVector::Resize growth requires default-constructible T");
#endif
        }
    }

    bool IsEmpty() const { return m_data.empty(); }
    bool IsFull() const { return m_data.size() == m_capacity; }

    std::size_t Size() const { return m_data.size(); }
    std::size_t Capacity() const { return m_capacity; }
    std::size_t FirstAvailableEntry() const { return m_data.size(); }

    template <typename... Args>
        requires std::constructible_from<T, Args...>
    bool Emplace(Args&&... aArgs)
    {
        if(IsFull()) { return false; }

        m_data.emplace_back(std::forward<Args>(aArgs)...);
        return true;
    }

    bool Append(T&& t)
    {
        return Emplace(std::move(t));
    }

    bool Append(const T& t)
    {
        return Emplace(t);
    }

    void Remove(std::size_t idx)
    {
#if defined(DAN_DEBUG) && DAN_DEBUG
        assert(idx < m_data.size());
#endif
        if(idx >= m_data.size()) { return; }

        const std::size_t lastValidIdx = m_data.size() - 1;
        if(idx != lastValidIdx)
        {
            std::swap(m_data[idx], m_data[lastValidIdx]);
        }

        m_data.pop_back();
    }

    /// Retrieves the const pointer for the element at the index.
    /// NOTE: If index is invalid, pointer will be nullptr.
    const T* Get(std::size_t idx) const
    {
        if(idx >= m_data.size()) { return nullptr; }
        return &m_data[idx];
    }

    /// Retrieves the pointer to a mutable reference for the element at the index.
    /// NOTE: If index is invalid, pointer will be nullptr.
    T* GetMutable(std::size_t idx)
    {
        if(idx >= m_data.size()) { return nullptr; }
        return &m_data[idx];
    }

    auto begin() noexcept { return m_data.begin(); }
    auto end() noexcept { return m_data.end(); }
    auto begin() const noexcept { return m_data.begin(); }
    auto end() const noexcept { return m_data.end(); }
    auto cbegin() const noexcept { return m_data.cbegin(); }
    auto cend() const noexcept { return m_data.cend(); }

    // We need one for const PopSwapVectors and one for non-const.
    template <typename ExecutionPolicy, typename F>
        requires std::is_execution_policy_v<std::remove_cvref_t<ExecutionPolicy>> &&
                 std::invocable<F&, T&>
    void ForEach(ExecutionPolicy&& policy, F&& aFunc)
    {
        std::for_each(
             std::forward<ExecutionPolicy>(policy),
               m_data.begin(),
               m_data.end(),
               [&aFunc](T& data) { aFunc(data); }
        );
    }

    // The const version
    template <typename ExecutionPolicy, typename F>
        requires std::is_execution_policy_v<std::remove_cvref_t<ExecutionPolicy>> &&
                 std::invocable<F&, const T&>
    void ForEach(ExecutionPolicy&& policy, F&& aFunc) const
    {
        std::for_each(
             std::forward<ExecutionPolicy>(policy),
               m_data.cbegin(),
               m_data.cend(),
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
    template<typename F> requires std::invocable<F&, T&>
    void ForEachParallel(F&& aFunc)
    {
        ForEach(std::execution::par, std::forward<F>(aFunc));
    }

    /// This is the standard, predictable behavior. It runs on the current 
    /// thread in order from the first element to the last.
    /// 
    /// @note Best used for small datasets or functions that are very fast.
    template<typename F> requires std::invocable<F&, T&>
    void ForEachSequential(F&& aFunc)
    {
        for(T& data : m_data)
        {
            aFunc(data);
        }
    }

    /// Runs on a single thread but allows the compiler to use special CPU 
    /// instructions to process multiple elements at once (instruction-level parallelism).
    /// 
    /// Your function must not perform synchronization (like locking a mutex), 
    /// as this can cause deadlocks in unsequenced mode.
    template<typename F> requires std::invocable<F&, T&>
    void ForEachUnsequenced(F&& aFunc)
    {
        ForEach(std::execution::unseq, std::forward<F>(aFunc));
    }


    /// Spreads work across multiple threads AND uses SIMD instructions. 
    /// This offers the highest potential performance for massive datasets.
    /// 
    /// Combines the risks of Parallel and Unsequenced: must be 
    /// thread-safe and must not use synchronization primitives.
    template<typename F> requires std::invocable<F&, T&>
    void ForEachParallelUnsequenced(F&& aFunc)
    {
        ForEach(std::execution::par_unseq, std::forward<F>(aFunc));
    }

    // Default to sequential for safety
    template<typename F> requires std::invocable<F&, T&>
    void ForEach(F&& aFunc)
    {
        ForEachSequential(std::forward<F>(aFunc));
    }
};
