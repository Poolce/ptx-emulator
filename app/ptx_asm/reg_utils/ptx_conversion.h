#pragma once

#include <ptx_types.h>
#include <reg_utils/ptx_type_traits.h>

namespace Emulator::Ptx::RegUtils::Conv
{

/// @brief Casts a register storage value to a corresponding C++ type.
/// @tparam ptxType Source PTX data type.
/// @param src Source register value.
/// @return Cast register value.
template <dataType ptxType, typename ValueType = TypeTraits::CType<ptxType>>
inline ValueType& Cast(void* pValue);
/// @brief Casts a register storage value to a corresponding C++ type.
/// @tparam ptxType Source PTX data type.
/// @param src Source register value.
/// @return Cast register value.
template <dataType ptxType, typename ValueType = TypeTraits::CType<ptxType>>
inline const ValueType& Cast(const void* pValue);

/// @brief Writes a value of a corresponding C++ type to a register storage.
/// @tparam ptxType Source PTX data type.
/// @param pDst Destination register storage.
/// @param value Value to be written.
template <dataType ptxType, typename ValueType = TypeTraits::CType<ptxType>>
inline void Write(void* pDst, const ValueType& value);

/// @brief Copies a value from one register storage to another.
/// @tparam ptxType PTX data type of the stored data.
/// @param pSrc Source register storage.
/// @param pDst Destination register storage.
template <dataType ptxType>
inline void Copy(const void* pSrc, void* pDst);

/// @brief Converts bytes of reg storage from one ptx type to another.
/// @tparam srcPtxType Source PTX data type.
/// @tparam dstPtxType Destination PTX data type.
/// @param src Source register value.
/// @return Converted register value.
template <dataType srcPtxType, dataType dstPtxType>
inline void Convert(const void* pSrc, void* pDst);

#pragma region detail

template <dataType ptxType, typename ValueType>
ValueType& Cast(void* pValue)
{
    __EMULATOR_PTX_REG_UTILS_TYPE_TRAITS_CHECK_VALUE_TYPE(ValueType);

    return *reinterpret_cast<ValueType*>(pValue);
}
template <dataType ptxType, typename ValueType>
const ValueType& Cast(const void* pValue)
{
    __EMULATOR_PTX_REG_UTILS_TYPE_TRAITS_CHECK_VALUE_TYPE(ValueType);

    return *reinterpret_cast<const ValueType*>(pValue);
}

template <dataType ptxType, typename ValueType>
void Write(void* pDst, const ValueType& value)
{
    __EMULATOR_PTX_REG_UTILS_TYPE_TRAITS_CHECK_VALUE_TYPE(ValueType);

    Cast<ptxType>(pDst) = value;
}

template <dataType ptxType>
void Copy(const void* pSrc, void* pDst)
{
    Cast<ptxType>(pDst) = Cast<ptxType>(pSrc);
}

template <dataType srcPtxType, dataType dstPtxType>
void Convert(const void* pSrc, void* pDst)
{
    static_assert(srcPtxType != dstPtxType,
                  "Source and destination types must be different. Use copy instead.");

    using DstType = TypeTraits::CType<dstPtxType>;
    __EMULATOR_PTX_REG_UTILS_TYPE_TRAITS_CHECK_VALUE_TYPE(DstType);

    Cast<dstPtxType>(pDst) = static_cast<DstType>(Cast<srcPtxType>(pSrc));
}

#pragma endregion detail

}  // namespace Emulator::Ptx::RegUtils::Conv
