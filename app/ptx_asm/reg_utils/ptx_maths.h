#pragma once

#include <ptx_types.h>
#include <reg_utils/ptx_type_traits.h>
#include <reg_utils/ptx_conversion.h>

namespace Emulator::Ptx::RegUtils::Maths
{

/// @brief Adds two register values of the same PTX data type.
/// @tparam ptxType Source PTX data type.
/// @param rhs Right-hand side register value.
/// @param lhs Left-hand side register value.
/// @return Result of the addition.
template <dataType ptxType>
inline void Add(const void* pLhs, const void* pRhs, void* pRet);

/// @brief Subtracts two register values of the same PTX data type.
/// @tparam ptxType Source PTX data type.
/// @param rhs Right-hand side register value.
/// @param lhs Left-hand side register value.
/// @param ret Result of the subtraction.
template <dataType ptxType>
inline void Sub(const void* pLhs, const void* pRhs, void* pRet);

/// @brief Multiplies two register values of the same PTX data type.
/// @tparam ptxType Source PTX data type.
/// @param rhs Right-hand side register value.
/// @param lhs Left-hand side register value.
/// @param ret Result of the multiplication.
template <dataType ptxType, mulmodeQl mulMode>
inline void Mul(const void* pLhs, const void* pRhs, void* pRet);

/// @brief Divides two register values of the same PTX data type.
/// @tparam ptxType PTX data type.
/// @param rhs Right-hand side register value.
/// @param lhs Left-hand side register value.
/// @param ret Result of the division.
template <dataType ptxType>
inline void Div(const void* pLhs, const void* pRhs, void* pRet);

// TODO: Other operations

#pragma region detail

namespace detail
{

// Binary

template <typename T>
struct __AddOp
{
    T operator()(const T& lhs, const T& rhs) const
    {
        return lhs + rhs;
    }
};

template <typename T>
struct __SubOp
{
    T operator()(const T& lhs, const T& rhs) const
    {
        return lhs - rhs;
    }
};

template <typename T>
struct __MulOp
{
    T operator()(const T& lhs, const T& rhs) const
    {
        return lhs * rhs;
    }
};

template <typename T>
struct __DivOp
{
    T operator()(const T& lhs, const T& rhs) const
    {
        return lhs / rhs;
    }
};

template <typename T>
struct __AndOp
{
    T operator()(const T& lhs, const T& rhs) const
    {
        return lhs & rhs;
    }
};

template <typename T>
struct __OrOp
{
    T operator()(const T& lhs, const T& rhs) const
    {
        return lhs | rhs;
    }
};

template <dataType ptxType, typename Op>
inline void __Apply(const void* pLhs, const void* pRhs, void* pDst)
{
    using Type = TypeTraits::CType<ptxType>;

    const Type& lhsValue = Conv::Cast<ptxType>(pLhs);
    const Type& rhsValue = Conv::Cast<ptxType>(pRhs);
    Type&       dstValue = Conv::Cast<ptxType>(pDst);

    dstValue = Op{}(lhsValue, rhsValue);
}

// Unary

template <typename T>
struct __XorOp
{
    T operator()(const T& lhs, const T& rhs) const
    {
        return lhs ^ rhs;
    }
};

template <typename T>
struct __NotOp
{
    T operator()(const T& value) const
    {
        return ~value;
    }
};

template <typename T>
struct __CNotOp
{
    T operator()(const T& value) const
    {
        return (value == 0) ? 1 : 0;
    }
};

template <dataType ptxType, typename Op>
inline void __Apply(const void* pSrc, void* pDst)
{
    using Type = TypeTraits::CType<ptxType>;

    const Type& srcValue = Conv::Cast<ptxType>(pSrc);
    Type&       dstValue = Conv::Cast<ptxType>(pDst);

    dstValue = Op{}(srcValue);
}

} // namespace detail

template <dataType ptxType>
void Add(const void* pLhs, const void* pRhs, void* pRet)
{
    detail::__Apply<ptxType, detail::__AddOp>(pRhs, pLhs, pRet);
}

template <dataType ptxType>
void Sub(const void* pLhs, const void* pRhs, void* pRet)
{
    detail::__Apply<ptxType, detail::__SubOp>(pRhs, pLhs, pRet);
}

template <dataType ptxType, mulmodeQl mulMode>
void Mul(const void* pLhs, const void* pRhs, void* pRet)
{
    constexpr dataType srcPtxType  = ptxType;
    constexpr dataType calcPtxType = TypeTraits::doubleOrSameType<srcPtxType>;
    using SrcType  = TypeTraits::CType<srcPtxType>;
    using CalcType = TypeTraits::CType<calcPtxType>;

    const CalcType lhsValue   = static_cast<CalcType>(Conv::Cast<srcPtxType>(pLhs));
    const CalcType rhsValue   = static_cast<CalcType>(Conv::Cast<srcPtxType>(pRhs));
    CalcType       tempResult = 0xDEADBEEFDEADBEEF;

    detail::__Apply<calcPtxType>(static_cast<const void*>(&lhsValue),
                                 static_cast<const void*>(&rhsValue),
                                 static_cast<void*>(&tempResult));

    if constexpr (mulMode == mulmodeQl::Wide)
    {
        Conv::Write<calcPtxType>(pRet, tempResult);
    }
    else if constexpr (mulMode == mulmodeQl::Hi)
    {
        static_assert(srcPtxType != calcPtxType,
                      "High multiplication mode requires a double type which is undefined.");

        // shift left to keep the high bits
        tempResult << (sizeof(SrcType) * 8);
        Conv::Write<srcPtxType>(pRet, static_cast<SrcType>(tempResult));
    } else if constexpr (mulMode == mulmodeQl::Lo)
    {
        // implictly keep the low bits by writing the value directly to a smaller type
        Conv::Write<srcPtxType>(pRet, static_cast<SrcType>(tempResult));
    }
}

template <dataType ptxType>
void Div(const void* pLhs, const void* pRhs, void* pRet)
{
    detail::__Apply<ptxType, detail::__DivOp>(pRhs, pLhs, pRet);
}

#pragma endregion detail

} // namespace Emulator::Ptx::RegUtils::Maths
