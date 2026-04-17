#pragma once

#include <functional>
#include <memory>
#include <variant>

template <typename T>
class DeferredAllocator
{
  public:
    using Object        = T;
    using AllocatorFunc = std::function<Object()>;

    template <std::enable_if_t<std::is_default_constructible_v<Object>, int> = 0>
    DeferredAllocator()
        : allocator_func_{[]() { return Object{}; }} {}

    explicit DeferredAllocator(AllocatorFunc allocator_func) noexcept
        : allocator_func_{std::move(allocator_func)} {}

    Object Allocate() { return allocator_func_(); }

    inline Object operator() () { return Allocate(); }

  private:
    AllocatorFunc allocator_func_;
};

template <typename T>
class Deferred
{
  public:
    using Object        = T;
    using Allocator     = DeferredAllocator<Object>;
    using AllocatorFunc = DeferredAllocator<Object>::AllocatorFunc;

    explicit Deferred(AllocatorFunc allocator_func) noexcept : data_(Allocator{std::move(allocator_func)}) {}

    Object& Get()
    {
        if (std::holds_alternative<Allocator>(data_))
        {
            data_ = std::get<Allocator>(data_).Allocate();
        }
        return std::get<T>(data_);
    }

    inline auto operator *  () { return  Get(); }
    inline auto operator -> () { return &Get(); }

  private:
    std::variant<Object, Allocator> data_;
};
