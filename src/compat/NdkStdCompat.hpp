#pragma once

// Only C++20+ translation units need atomic_ref; leave older units (and their
// feature-test-macro sensitive system headers) untouched.
#if __cplusplus >= 202002L

#include <atomic>
#include <type_traits>

// Android NDK r28 ships Clang 19, which Geode 5.9 requires, but its libc++
// does not yet expose C++20 std::atomic_ref. Arc (a Geode dependency) uses
// atomic_ref only for trivially-copyable worker state. This small compatible
// implementation keeps those accesses atomic using Clang's builtins.
#if defined(__ANDROID__) && !defined(__cpp_lib_atomic_ref)
namespace std {
template <class T>
class atomic_ref {
    static_assert(is_trivially_copyable_v<T>);

public:
    using value_type = T;
    static constexpr bool is_always_lock_free = __atomic_always_lock_free(sizeof(T), nullptr);
    static constexpr size_t required_alignment = alignof(T);

    explicit atomic_ref(T& value) noexcept : m_value(&value) {}
    atomic_ref(atomic_ref const&) noexcept = default;

    void store(T desired, memory_order order = memory_order_seq_cst) const noexcept {
        __atomic_store(m_value, &desired, static_cast<int>(order));
    }

    T load(memory_order order = memory_order_seq_cst) const noexcept {
        T result{};
        __atomic_load(m_value, &result, static_cast<int>(order));
        return result;
    }

    operator T() const noexcept {
        return load();
    }

private:
    T* m_value;
};

template <class T>
atomic_ref(T&) -> atomic_ref<T>;
} // namespace std

#define __cpp_lib_atomic_ref 201806L
#endif
#endif // __cplusplus >= 202002L
