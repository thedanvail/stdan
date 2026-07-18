#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>

namespace stdan::storage {
/// Named and typed class to emphasize when a pointer into
/// one of the containers may become invalid incredibly quickly.
template<class T>
class [[nodiscard]] transient_ptr {
public:
    transient_ptr() noexcept = default;
    explicit transient_ptr(T* ptr) noexcept
        : ptr_(ptr){}

    explicit transient_ptr(T& ref) noexcept
        : ptr_(std::addressof(ref)){}

    transient_ptr(std::nullptr_t) noexcept
        : ptr_(nullptr){}

    transient_ptr(const transient_ptr&) = delete;
    transient_ptr& operator=(const transient_ptr&) = delete;

    transient_ptr(transient_ptr&&) = delete;
    transient_ptr& operator=(transient_ptr&&) = delete;

    ~transient_ptr() = default;

    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    friend bool operator==(const transient_ptr& lhs, std::nullptr_t) noexcept { return lhs.ptr_ == nullptr; }
    // Take T by const reference: MSVC rejects by-value parameters whose
    // alignment exceeds what the calling convention can guarantee (C2719),
    // which shows up for over-aligned T such as alignas(8192).
    friend bool operator==(const transient_ptr& lhs, const T& rhs) noexcept
        requires std::equality_comparable<T> {
        if(lhs.ptr_ == nullptr) { return false; }
        return *lhs.ptr_ == rhs;
    }
    
    bool equals(const T& t) const requires std::equality_comparable<T> {
        if(ptr_ == nullptr) { return false; }
        return *ptr_ == t;
    }

    T& operator*() & = delete;
    T* operator->() & = delete;

    const T& operator*() const& = delete;
    const T* operator->() const& = delete;

    const T& operator*() const&& = delete;
    const T* operator->() const&& = delete;


    // Allow ops on rvalue `transient_ptr`
    // though this would allow someone to do 
    // ```cpp
    // auto p = container.get(id);
    // std::move(p)->foo();
    // ```
    // which is not supported but I cannot prevent it.
    template<typename Self> requires (!std::is_lvalue_reference_v<Self>)
    T& operator*(this Self&& self) noexcept
    {
        assert(self.ptr_ != nullptr);
        return *(self.ptr_);
    }

    template <typename Self> requires (!std::is_lvalue_reference_v<Self>)
    T* operator->(this Self&& self) noexcept
    {
        assert(self.ptr_ != nullptr);
        return self.ptr_;
    }

    /// Runs the passed-in function with the associated T*.
    /// Callers must check for null inside the function being invoked.
    template<typename Self, std::invocable<T*> Func>
    requires (!std::is_lvalue_reference_v<Self> && !std::is_const_v<std::remove_reference_t<Self>>)
    std::invoke_result_t<Func, T*> apply(this Self&& self, Func&& func)
    {
        return std::invoke(std::forward<Func>(func), self.ptr_);
    }

    /// Runs the passed-in function with the associated T*.
    /// Assumes non-null T* and passes T& directly
    template<typename Self, std::invocable<T&> Func>
    requires (!std::is_lvalue_reference_v<Self> && !std::is_const_v<std::remove_reference_t<Self>>)
    std::invoke_result_t<Func, T&> apply_non_null(this Self&& self, Func&& func)
    {
        assert(self.ptr_ != nullptr);
        return std::invoke(std::forward<Func>(func), *(self.ptr_));
    }

    /// Runs the passed-in function with the associated T&.
    /// Conditionally runs only if valid pointer and then passes T&
    template <typename Self, std::invocable<T&> Func>
    requires (!std::is_lvalue_reference_v<Self> && !std::is_const_v<std::remove_reference_t<Self>>)
    bool apply_if_non_null(this Self&& self, Func&& func)
    {
        if (self.ptr_ != nullptr) {
            std::invoke(std::forward<Func>(func), *(self.ptr_));
            return true;
        }
        return false;
    }

private:
    T* ptr_ = nullptr;
};
}
