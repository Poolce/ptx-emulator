#pragma once

#include <ptx_types.h>

#include <cstdint>
#include <stdfloat>
#include <type_traits>

#define __CPP_FIXED_FLOAT_LIBRARY_SUPPORT (__STDCPP_FLOAT16_T__ == 1)

namespace Emulator::Ptx::RegUtils::TypeTraits
{

#pragma region detail

namespace detail
{

struct __UndefinedType {};

template <typename T>
constexpr bool __isUndefined = std::is_same_v<T, __UndefinedType>;

template <dataType ptxType>
using __CType =
    // floating point types
#if __CPP_FIXED_FLOAT_LIBRARY_SUPPORT
    std::conditional_t<ptxType == dataType::F16,    std::float16_t,
    // TODO: std::conditional_t<ptxType == dataType::F16X2,  std::bfloat16_t,
    std::conditional_t<ptxType == dataType::F32,    std::float32_t,
    std::conditional_t<ptxType == dataType::F64,    std::float64_t,
#else
    std::conditional_t<ptxType == dataType::F16,    float,
    // TODO: std::conditional_t<ptxType == dataType::F16X2,  float,
    std::conditional_t<ptxType == dataType::F32,    std::conditional_t<sizeof(float) == 4, float, double>,
    std::conditional_t<ptxType == dataType::F64,    double,
#endif

    // signed integer types
    std::conditional_t<ptxType == dataType::S8,     int8_t,
    std::conditional_t<ptxType == dataType::S16,    int16_t,
    std::conditional_t<ptxType == dataType::S32,    int32_t,
    std::conditional_t<ptxType == dataType::S64,    int64_t,

    // unsigned integer types
    std::conditional_t<ptxType == dataType::U8,     uint8_t,
    std::conditional_t<ptxType == dataType::U16,    uint16_t,
    std::conditional_t<ptxType == dataType::U32,    uint32_t,
    std::conditional_t<ptxType == dataType::U64,    uint64_t,

    // byte types
    std::conditional_t<ptxType == dataType::B8,     uint8_t,
    std::conditional_t<ptxType == dataType::B16,    uint16_t,
    std::conditional_t<ptxType == dataType::B32,    uint32_t,
    std::conditional_t<ptxType == dataType::B64,    uint64_t,
    // TODO: dataType::B128

    // default value
    __UndefinedType>>>>>>>>>>>>>>>;

template <dataType ptxType>
constexpr bool __isFloatingPoint =
    (ptxType == dataType::F16)   ||
    // TODO: (ptxType == dataType::F16X2) ||
    (ptxType == dataType::F32)   ||
    (ptxType == dataType::F64);

template <dataType ptxType>
constexpr bool __isUnsignedInteger =
    (ptxType == dataType::U8)  ||
    (ptxType == dataType::U16) ||
    (ptxType == dataType::U32) ||
    (ptxType == dataType::U64);

template <dataType ptxType>
constexpr bool __isSignedInteger =
    (ptxType == dataType::S8)  ||
    (ptxType == dataType::S16) ||
    (ptxType == dataType::S32) ||
    (ptxType == dataType::S64);

template <dataType ptxType>
constexpr bool __isByte =
    (ptxType == dataType::B8)  ||
    (ptxType == dataType::B16) ||
    (ptxType == dataType::B32) ||
    (ptxType == dataType::B64);
    // TODO: (ptxType == dataType::B128)

template <dataType ptxType>
constexpr bool __isPredicate = (ptxType == dataType::Pred);

template <dataType ptxType>
constexpr dataType __GetDoublePtxType()
{
    // floating point types
    if constexpr (ptxType == dataType::F16) return dataType::F32;
    if constexpr (ptxType == dataType::F32) return dataType::F64;

    // signed integer types
    if constexpr (ptxType == dataType::S8)  return dataType::S16;
    if constexpr (ptxType == dataType::S16) return dataType::S32;
    if constexpr (ptxType == dataType::S32) return dataType::S64;

    // unsigned integer types
    if constexpr (ptxType == dataType::U8)  return dataType::U16;
    if constexpr (ptxType == dataType::U16) return dataType::U32;
    if constexpr (ptxType == dataType::U32) return dataType::U64;

    // byte types
    if constexpr (ptxType == dataType::B8)  return dataType::B16;
    if constexpr (ptxType == dataType::B16) return dataType::B32;
    if constexpr (ptxType == dataType::B32) return dataType::B64;
    // TODO: if constexpr (ptxType == dataType::B64) return dataType::B128;

    return dataType::UNDEFINED;
}

template <dataType ptxType>
constexpr bool __IsFinalDoubleType()
{
    // floating point types
    if constexpr (ptxType == dataType::F64) return true;

    // signed integer types
    if constexpr (ptxType == dataType::S64) return true;

    // unsigned integer types
    if constexpr (ptxType == dataType::U64) return true;

    // unsigned integer types
    if constexpr (ptxType == dataType::B64) return true; // TODO: B128

    return false;
}

#define __EMULATOR_PTX_REG_UTILS_TYPE_TRAITS_CHECK_VALUE_TYPE(type)                    \
    static_assert(!::Emulator::Ptx::RegUtils::TypeTraits::detail::__isUndefined<type>, \
                  "Undefined type")

} // namespace detail

#pragma endregion detail

/// @brief Retuns the corresponding C++ type for a given PTX data type.
/// @tparam ptxType Source PTX data type.
template <dataType ptxType>
using CType = detail::__CType<ptxType>;

/// @brief Retuns the corresponding PTX data type which is the "double" of a given PTX data type.
/// @tparam ptxType Source PTX data type.
template <dataType ptxType>
constexpr dataType doubleType = detail::__GetDoublePtxType<ptxType>();

/// @brief Returns the corresponding PTX data type which is the "double" of a given PTX data type, or the same type if no double type exists.
/// @tparam ptxType
template <dataType ptxType>
constexpr dataType doubleOrSameType =
    (detail::__IsFinalDoubleType<ptxType>()) ? ptxType : doubleType<ptxType>;

/// @brief Checks if a PTX data type is a floating-point type.
/// @tparam ptxType Source PTX data type.
template <dataType ptxType>
constexpr bool isFloatingPoint = detail::__isFloatingPoint<ptxType>;

/// @brief Checks if a PTX data type is an integer type (signed or unsigned).
/// @tparam ptxType Source PTX data type.
template <dataType ptxType>
constexpr bool isInteger = detail::__isUnsignedInteger<ptxType> ||
                           detail::__isSignedInteger<ptxType>;

/// @brief Checks if a PTX data type is an unsigned integer type.
/// @tparam ptxType Source PTX data type.
template <dataType ptxType>
constexpr bool isUnsigned = detail::__isUnsignedInteger<ptxType>;

/// @brief Checks if a PTX data type is a signed integer type.
/// @tparam ptxType Source PTX data type.
template <dataType ptxType>
constexpr bool isSigned = detail::__isSignedInteger<ptxType>;

/// @brief Checks if a PTX data type is a byte type.
/// @tparam ptxType Source PTX data type.
template <dataType ptxType>
constexpr bool isByte = detail::__isByte<ptxType>;

/// @brief Checks if a PTX data type is a predicate type.
/// @tparam ptxType Source PTX data type.
template <dataType ptxType>
constexpr bool isPredicate = detail::__isPredicate<ptxType>;

} // namespace Emulator::Ptx::RegUtils::TypeTraits
