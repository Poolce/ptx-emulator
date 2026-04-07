#pragma once

#include <ptx_types.h>
#include <reg_utils/ptx_conversion.h>
#include <reg_utils/ptx_maths.h>
#include <reg_utils/ptx_type_traits.h>

namespace Emulator::Ptx::RegUtils::RegStorage
{

/// @brief Type of the universal storage for a register which can hold any
/// supported register value.
using RegStorageType = uint64_t;

/// @brief Class representing the storage of a register which can hold any supported register value.
/// Is provided by operators for basic arithmetic operations on the stored value.
class RegStorage {

  // Intilization
  public:

    RegStorage()  noexcept = default;
    ~RegStorage() noexcept = default;

    explicit RegStorage(RegStorageType storage) noexcept : storage_{storage} {}
    RegStorage& operator = (RegStorageType storage) noexcept
    {
        storage_ = storage;
        return *this;
    }

    template <dataType ptxType, typename ValueType = TypeTraits::CType<ptxType>>
    RegStorage(const ValueType& value) noexcept
    {
        Set<ptxType>(value);
    }
    template <dataType ptxType, typename ValueType = TypeTraits::CType<ptxType>>
    RegStorage& operator = (const ValueType& value) noexcept
    {
        Set<ptxType>(value);
        return *this;
    }

    template <dataType ptxType>
    RegStorage(void* pValue) noexcept {
        Conv::Copy<ptxType>(pValue, Data());
    }

    RegStorage(const RegStorage& other) noexcept            = default;
    RegStorage& operator=(const RegStorage& other) noexcept = default;

    RegStorage(RegStorage&& other) noexcept : storage_{other.Release()} {}
    RegStorage& operator=(RegStorage&& other) noexcept
    {
        if (this != &other)
        {
            storage_ = other.Release();
        }
        return *this;
    }

  // Accessors
  public:

    template <dataType ptxType>
    [[nodiscard]] inline TypeTraits::CType<ptxType>&       Get()       { return Conv::Cast<ptxType>(Data()); }
    template <dataType ptxType>
    [[nodiscard]] inline const TypeTraits::CType<ptxType>& Get() const { return Conv::Cast<ptxType>(Data()); }

    template <dataType ptxType, typename ValueType = TypeTraits::CType<ptxType>>
    void Set(const ValueType& value)
    {
        Conv::Write<ptxType>(Data(), value);
    }

    [[nodiscard]] RegStorageType Release() noexcept
    {
        RegStorageType ret = storage_;
        Reset();
        return ret;
    }

  // Internal
  private:

    void Reset() noexcept { storage_ = kUndefinedValue; }

    [[nodiscard]] inline void*       Data() noexcept       { return &storage_; }
    [[nodiscard]] inline const void* Data() const noexcept { return &storage_; }

    RegStorageType storage_ = kUndefinedValue;

    static constexpr RegStorageType kUndefinedValue = 0xDEADBEEFDEADBEEF;
}; // class RegStorage

/// @brief Performs addition operation.
template <dataType ptxType>
static RegStorage Add(const RegStorage& lhs, const RegStorage& rhs)
{
    RegStorage result;
    Maths::Add<ptxType>(lhs.Data(), rhs.Data(), result.Data());
    return result;
}

/// @brief Performs subtraction operation.
template <dataType ptxType>
static RegStorage Sub(const RegStorage& lhs, const RegStorage& rhs)
{
    RegStorage result;
    Maths::Sub<ptxType>(lhs.Data(), rhs.Data(), result.Data());
    return result;
}

/// @brief Performs multiplication operation with mode.
template <dataType ptxType, mulmodeQl mulMode>
static RegStorage Mul(const RegStorage& lhs, const RegStorage& rhs)
{
    RegStorage result;
    Maths::Mul<ptxType, mulMode>(lhs.Data(), rhs.Data(), result.Data());
    return result;
}

/// @brief Performs division operation.
template <dataType ptxType>
static RegStorage Div(const RegStorage& lhs, const RegStorage& rhs)
{
    RegStorage result;
    Maths::Div<ptxType>(lhs.Data(), rhs.Data(), result.Data());
    return result;
}

// TODO: Other operations

/// @brief Wrapper register storage class for passing modifiable registers to instruction handlers.
/// Provided in-memory register value is automatilly evaluated on destroy.
class RegStorageWrapper
{
  public:

    explicit RegStorageWrapper(RegStorageType& reg) noexcept
        : pReg_{&reg}
        , storage_{*pReg_} {}

    ~RegStorageWrapper() noexcept { Release(); }

    RegStorageWrapper(RegStorageWrapper&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            pReg_ = other.pReg_;
            other.pReg_ = nullptr;
        }
    }
    RegStorageWrapper& operator=(RegStorageWrapper&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            pReg_ = other.pReg_;
            other.pReg_ = nullptr;
        }
        return *this;
    }

    RegStorageWrapper() = delete;
    RegStorageWrapper(const RegStorageWrapper& other) noexcept = delete;
    RegStorageWrapper& operator=(const RegStorageWrapper& other) noexcept = delete;

    inline operator RegStorage&()       noexcept       { return storage_; }
    inline operator const RegStorage&() const noexcept { return storage_; }

  private:

    void Release() noexcept
    {
        *pReg_ = storage_.Release();
    }

    RegStorageType* pReg_ = nullptr;

    RegStorage storage_;
}; // class RegStorageWrapper

/**
 * Usage example:
 *
 *    // For some intruction, e.g.
 *    // "add.u32 r1, r2, r3;"
 *
 *
 *          // Auto-generated thread instruction handler
 *          ...
 *
 *          uint64_t&       r1Data; // it's read from memory for "r1" register
 *          const uint64_t& r2Data; // for "r2" register
 *          const uint64_t& r3Data; // for "r3" register
 *          // this `uint64_t` is the `RegStorage::RegStorageType`
 *
 *          RegStorage::RegStorageWrapper r1{r1Data}; // create wrapper for "r1" register
 *          const RegStorage::RegStorage  r2{r2Data}; // ... "r2"
 *          const RegStorage::RegStorage  r3{r3Data}; // ... "r3"
 *          // it is also possible to make all three registers as non-const wrappers for generality
 *
 *    ----  Add<...>(r1, r2, r3); // call the customizable instruction handler
 *    |     // on handler destroy, `r1Data` is implicitly evaluated with the value in `r1`
 *    |
 *    |
 *    |              // Customizable instruction handler for "add" instruction
 *    |              template <..., dataType ptxType, ...>
 *    ----------->   void Add(RegStorage::RegStorage&       dst,
 *                            const RegStorage::RegStorage& src1,
 *                            const RegStorage::RegStorage& src2)
 *                   {
 *                       dst = RegStorage::Add<ptxType>(src1, src2);
 *                   }
 */

} // namespace Emulator::Ptx::RegUtils::RegStorage
