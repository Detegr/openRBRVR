#pragma once

#include <MinHook.h>
#include <stdexcept>

// RAII wrapper for MinHook
template <typename T>
struct Hook {
    T call;
    T src;

    explicit Hook()
        : call(nullptr)
        , src(nullptr)
    {
    }

    explicit Hook(T src, T tgt)
        : src(src)
    {
        if (MH_CreateHook(reinterpret_cast<void*>(src), reinterpret_cast<void*>(tgt), reinterpret_cast<void**>(&call)) != MH_OK) {
            throw std::runtime_error("Could not hook");
        }
        enable();
    }
    void enable()
    {
        if (MH_EnableHook(reinterpret_cast<void*>(src)) != MH_OK) {
            throw std::runtime_error("Could not enable hook");
        }
    }
    void disable()
    {
        if (MH_DisableHook(reinterpret_cast<void*>(src)) != MH_OK) {
            throw std::runtime_error("Could not disable hook");
        }
    }
    Hook(const Hook&) = delete;
    Hook(Hook&& rhs)
    {
        call = rhs.call;
        src = rhs.src;
        rhs.call = nullptr;
        rhs.src = nullptr;
    }
    Hook& operator=(const Hook&) = delete;
    Hook& operator=(Hook&& rhs) noexcept
    {
        call = rhs.call;
        src = rhs.src;
        rhs.call = nullptr;
        rhs.src = nullptr;
        return *this;
    }
    ~Hook()
    {
        if (src)
            MH_RemoveHook(reinterpret_cast<void*>(src));

        call = nullptr;
        src = nullptr;
    }
};

template <typename T, typename F>
T* get_vtable(F* obj)
{
    return (T*)(*(uintptr_t*)obj);
}
