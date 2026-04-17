#pragma once

#include <functional>
#include <memory>
#include <variant>

template <typename T>
class DeferredAllocator
{
  public:
    using Object    = T;
    using Allocator = std::function<Object()>;

    explicit DeferredAllocator(Allocator allocator) noexcept :
        allocator_{std::move(allocator)} {}

    Object Allocate() { return allocator_(); }

    Object operator() () { return Allocate(); }

  private:
    Allocator allocator_;
};

template <typename T>
class Deferred
{
  public:
    using Object    = T;
    using Allocator = DeferredAllocator<Object>::Allocator;

    explicit Deferred(Allocator allocator) noexcept : data_(std::move(allocator)) {}

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
    std::variant<Object, DeferredAllocator<Object>> data_;
};

template <typename T>
class DeferredShared : public Deferred<std::shared_ptr<T>>
{
  public:
    using Super = Deferred<std::shared_ptr<T>>;

    using Object  = T;
    using Creator = Super::Allocator;

    explicit DeferredShared(Creator creator) noexcept
        : Super([creator_ = std::move(creator)]()
                {
                    return std::make_shared<Object>(creator_());
                })
    {}

    std::shared_ptr<T> GetShared() { return Super::Get(); }
    T& Get() { return *Super::Get(); }

    inline auto operator *  () { return  Get(); }
    inline auto operator -> () { return &Get(); }

    void Release() { Super::Get().reset(); }
};
