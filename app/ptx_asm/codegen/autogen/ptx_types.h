#pragma once

#include "utils.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <stdfloat>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// NOLINTBEGIN

namespace Emulator
{
namespace Ptx
{

enum class dataType : uint32_t
{
    UNDEFINED,
    F16,
    F32,
    F64,
    S8,
    S16,
    S32,
    S64,
    U8,
    U16,
    U32,
    U64,
    B8,
    B16,
    B32,
    B64,
    Pred,
    COUNT = 17
};
enum class registerType : uint32_t
{
    UNDEFINED,
    F,
    Fd,
    P,
    R,
    Rd,
    Spr,
    COUNT = 7
};
enum class sprType : uint32_t
{
    UNDEFINED,
    TidX,
    TidY,
    TidZ,
    CtaidX,
    CtaidY,
    CtaidZ,
    NtidX,
    NtidY,
    NtidZ,
    COUNT = 10
};

enum class pragmavalQl : uint32_t
{
    UNDEFINED,
    Nounroll,
    Used_bytes_mask,
    COUNT = 3
};

enum class cvtaspaceQl : uint32_t
{
    UNDEFINED,
    Const,
    Global,
    Local,
    Param,
    ParamEntry,
    Shared,
    SharedCta,
    SharedCluster,
    COUNT = 9
};

enum class setpcmpQl : uint32_t
{
    UNDEFINED,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    Lo,
    Ls,
    Hi,
    Hs,
    Equ,
    Neu,
    Ltu,
    Leu,
    Gtu,
    Geu,
    Num,
    Nan,
    COUNT = 19
};

enum class setpboolQl : uint32_t
{
    UNDEFINED,
    And,
    Or,
    Xor,
    COUNT = 4
};

enum class mulmodeQl : uint32_t
{
    UNDEFINED,
    Hi,
    Lo,
    Wide,
    COUNT = 4
};

enum class barmodeQl : uint32_t
{
    UNDEFINED,
    Sync,
    Arrive,
    COUNT = 3
};

enum class stspaceQl : uint32_t
{
    UNDEFINED,
    Const,
    Global,
    Local,
    Param,
    ParamEntry,
    ParamFunc,
    Shared,
    SharedCta,
    SharedCluster,
    COUNT = 10
};

enum class fmamodeQl : uint32_t
{
    UNDEFINED,
    Rn,
    Rz,
    Rm,
    Rp,
    COUNT = 5
};

enum class ldspaceQl : uint32_t
{
    UNDEFINED,
    Const,
    Global,
    Local,
    Param,
    ParamEntry,
    ParamFunc,
    Shared,
    SharedCta,
    SharedCluster,
    COUNT = 10
};

enum class madmodeQl : uint32_t
{
    UNDEFINED,
    Hi,
    Lo,
    Wide,
    COUNT = 4
};

enum class cvtmodeQl : uint32_t
{
    UNDEFINED,
    Rtz,
    Rm,
    Rz,
    Rn,
    Rp,
    COUNT = 6
};

template <typename T>
struct ParentType;

template <typename ChildType, typename ParentType = typename ParentType<ChildType>::type>
constexpr ParentType GetParent(ChildType child) requires std::is_enum_v<ChildType>
{
    throw std::invalid_argument("No parent mapping defined for the given child enum type.");
}

enum class localAbsDataType : uint32_t
{
    UNDEFINED,
    S32,
    S64,
    F32,
    F64,
    COUNT = 5
};

template <>
struct ParentType<localAbsDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localAbsDataType child)
{
    static constexpr std::array<dataType, 5> map{dataType::UNDEFINED,
                                                 dataType::S32,
                                                 dataType::S64,
                                                 dataType::F32,
                                                 dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localCvtaDataType : uint32_t
{
    UNDEFINED,
    U32,
    U64,
    COUNT = 3
};

template <>
struct ParentType<localCvtaDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localCvtaDataType child)
{
    static constexpr std::array<dataType, 3> map{dataType::UNDEFINED, dataType::U32, dataType::U64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localSetpDataType : uint32_t
{
    UNDEFINED,
    B16,
    B32,
    B64,
    U16,
    U32,
    U64,
    S16,
    S32,
    S64,
    F32,
    F64,
    COUNT = 12
};

template <>
struct ParentType<localSetpDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localSetpDataType child)
{
    static constexpr std::array<dataType, 12> map{dataType::UNDEFINED,
                                                  dataType::B16,
                                                  dataType::B32,
                                                  dataType::B64,
                                                  dataType::U16,
                                                  dataType::U32,
                                                  dataType::U64,
                                                  dataType::S16,
                                                  dataType::S32,
                                                  dataType::S64,
                                                  dataType::F32,
                                                  dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localAddDataType : uint32_t
{
    UNDEFINED,
    U16,
    U32,
    U64,
    S16,
    S32,
    S64,
    F32,
    F64,
    COUNT = 9
};

template <>
struct ParentType<localAddDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localAddDataType child)
{
    static constexpr std::array<dataType, 9> map{dataType::UNDEFINED,
                                                 dataType::U16,
                                                 dataType::U32,
                                                 dataType::U64,
                                                 dataType::S16,
                                                 dataType::S32,
                                                 dataType::S64,
                                                 dataType::F32,
                                                 dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localMovDataType : uint32_t
{
    UNDEFINED,
    Pred,
    B16,
    B32,
    B64,
    U16,
    U32,
    U64,
    S16,
    S32,
    S64,
    F32,
    F64,
    COUNT = 13
};

template <>
struct ParentType<localMovDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localMovDataType child)
{
    static constexpr std::array<dataType, 13> map{dataType::UNDEFINED,
                                                  dataType::Pred,
                                                  dataType::B16,
                                                  dataType::B32,
                                                  dataType::B64,
                                                  dataType::U16,
                                                  dataType::U32,
                                                  dataType::U64,
                                                  dataType::S16,
                                                  dataType::S32,
                                                  dataType::S64,
                                                  dataType::F32,
                                                  dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localShlDataType : uint32_t
{
    UNDEFINED,
    B16,
    B32,
    B64,
    COUNT = 4
};

template <>
struct ParentType<localShlDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localShlDataType child)
{
    static constexpr std::array<dataType, 4> map{dataType::UNDEFINED, dataType::B16, dataType::B32, dataType::B64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localAndDataType : uint32_t
{
    UNDEFINED,
    Pred,
    B16,
    B32,
    B64,
    COUNT = 5
};

template <>
struct ParentType<localAndDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localAndDataType child)
{
    static constexpr std::array<dataType, 5> map{dataType::UNDEFINED,
                                                 dataType::Pred,
                                                 dataType::B16,
                                                 dataType::B32,
                                                 dataType::B64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localMulDataType : uint32_t
{
    UNDEFINED,
    F16,
    F32,
    F64,
    U16,
    U32,
    U64,
    S16,
    S32,
    S64,
    COUNT = 10
};

template <>
struct ParentType<localMulDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localMulDataType child)
{
    static constexpr std::array<dataType, 10> map{dataType::UNDEFINED,
                                                  dataType::F16,
                                                  dataType::F32,
                                                  dataType::F64,
                                                  dataType::U16,
                                                  dataType::U32,
                                                  dataType::U64,
                                                  dataType::S16,
                                                  dataType::S32,
                                                  dataType::S64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localEx2DataType : uint32_t
{
    UNDEFINED,
    F16,
    F32,
    F64,
    COUNT = 4
};

template <>
struct ParentType<localEx2DataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localEx2DataType child)
{
    static constexpr std::array<dataType, 4> map{dataType::UNDEFINED, dataType::F16, dataType::F32, dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localStDataType : uint32_t
{
    UNDEFINED,
    B8,
    B16,
    B32,
    B64,
    U8,
    U16,
    U32,
    U64,
    S8,
    S16,
    S32,
    S64,
    F32,
    F64,
    COUNT = 15
};

template <>
struct ParentType<localStDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localStDataType child)
{
    static constexpr std::array<dataType, 15> map{dataType::UNDEFINED,
                                                  dataType::B8,
                                                  dataType::B16,
                                                  dataType::B32,
                                                  dataType::B64,
                                                  dataType::U8,
                                                  dataType::U16,
                                                  dataType::U32,
                                                  dataType::U64,
                                                  dataType::S8,
                                                  dataType::S16,
                                                  dataType::S32,
                                                  dataType::S64,
                                                  dataType::F32,
                                                  dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localFmaDataType : uint32_t
{
    UNDEFINED,
    F32,
    F64,
    COUNT = 3
};

template <>
struct ParentType<localFmaDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localFmaDataType child)
{
    static constexpr std::array<dataType, 3> map{dataType::UNDEFINED, dataType::F32, dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localNegDataType : uint32_t
{
    UNDEFINED,
    S16,
    S32,
    S64,
    COUNT = 4
};

template <>
struct ParentType<localNegDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localNegDataType child)
{
    static constexpr std::array<dataType, 4> map{dataType::UNDEFINED, dataType::S16, dataType::S32, dataType::S64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localSubDataType : uint32_t
{
    UNDEFINED,
    F32,
    S64,
    F64,
    COUNT = 4
};

template <>
struct ParentType<localSubDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localSubDataType child)
{
    static constexpr std::array<dataType, 4> map{dataType::UNDEFINED, dataType::F32, dataType::S64, dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localLdDataType : uint32_t
{
    UNDEFINED,
    B8,
    B16,
    B32,
    B64,
    U8,
    U16,
    U32,
    U64,
    S8,
    S16,
    S32,
    S64,
    F32,
    F64,
    COUNT = 15
};

template <>
struct ParentType<localLdDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localLdDataType child)
{
    static constexpr std::array<dataType, 15> map{dataType::UNDEFINED,
                                                  dataType::B8,
                                                  dataType::B16,
                                                  dataType::B32,
                                                  dataType::B64,
                                                  dataType::U8,
                                                  dataType::U16,
                                                  dataType::U32,
                                                  dataType::U64,
                                                  dataType::S8,
                                                  dataType::S16,
                                                  dataType::S32,
                                                  dataType::S64,
                                                  dataType::F32,
                                                  dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localRcpDataType : uint32_t
{
    UNDEFINED,
    F16,
    F32,
    F64,
    COUNT = 4
};

template <>
struct ParentType<localRcpDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localRcpDataType child)
{
    static constexpr std::array<dataType, 4> map{dataType::UNDEFINED, dataType::F16, dataType::F32, dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localTanhDataType : uint32_t
{
    UNDEFINED,
    F32,
    F64,
    COUNT = 3
};

template <>
struct ParentType<localTanhDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localTanhDataType child)
{
    static constexpr std::array<dataType, 3> map{dataType::UNDEFINED, dataType::F32, dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localMadDataType : uint32_t
{
    UNDEFINED,
    U16,
    U32,
    U64,
    S16,
    S32,
    S64,
    COUNT = 7
};

template <>
struct ParentType<localMadDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localMadDataType child)
{
    static constexpr std::array<dataType, 7> map{dataType::UNDEFINED,
                                                 dataType::U16,
                                                 dataType::U32,
                                                 dataType::U64,
                                                 dataType::S16,
                                                 dataType::S32,
                                                 dataType::S64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localCopysignDataType : uint32_t
{
    UNDEFINED,
    F32,
    F64,
    COUNT = 3
};

template <>
struct ParentType<localCopysignDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localCopysignDataType child)
{
    static constexpr std::array<dataType, 3> map{dataType::UNDEFINED, dataType::F32, dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localSelpDataType : uint32_t
{
    UNDEFINED,
    B16,
    B32,
    B64,
    U16,
    U32,
    U64,
    S16,
    S32,
    S64,
    F32,
    F64,
    COUNT = 12
};

template <>
struct ParentType<localSelpDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localSelpDataType child)
{
    static constexpr std::array<dataType, 12> map{dataType::UNDEFINED,
                                                  dataType::B16,
                                                  dataType::B32,
                                                  dataType::B64,
                                                  dataType::U16,
                                                  dataType::U32,
                                                  dataType::U64,
                                                  dataType::S16,
                                                  dataType::S32,
                                                  dataType::S64,
                                                  dataType::F32,
                                                  dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localCvtSrcDataType : uint32_t
{
    UNDEFINED,
    U8,
    U16,
    U32,
    U64,
    S8,
    S16,
    S32,
    S64,
    F16,
    F32,
    F64,
    COUNT = 12
};

template <>
struct ParentType<localCvtSrcDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localCvtSrcDataType child)
{
    static constexpr std::array<dataType, 12> map{dataType::UNDEFINED,
                                                  dataType::U8,
                                                  dataType::U16,
                                                  dataType::U32,
                                                  dataType::U64,
                                                  dataType::S8,
                                                  dataType::S16,
                                                  dataType::S32,
                                                  dataType::S64,
                                                  dataType::F16,
                                                  dataType::F32,
                                                  dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

enum class localCvtDstDataType : uint32_t
{
    UNDEFINED,
    U8,
    U16,
    U32,
    U64,
    S8,
    S16,
    S32,
    S64,
    F16,
    F32,
    F64,
    COUNT = 12
};

template <>
struct ParentType<localCvtDstDataType>
{
    using type = dataType;
};

template <>
constexpr dataType GetParent(localCvtDstDataType child)
{
    static constexpr std::array<dataType, 12> map{dataType::UNDEFINED,
                                                  dataType::U8,
                                                  dataType::U16,
                                                  dataType::U32,
                                                  dataType::U64,
                                                  dataType::S8,
                                                  dataType::S16,
                                                  dataType::S32,
                                                  dataType::S64,
                                                  dataType::F16,
                                                  dataType::F32,
                                                  dataType::F64};
    const auto idx = static_cast<size_t>(child);
    return map[idx];
}

class symbolOperand : public std::string
{
  public:
    symbolOperand() = default;
    symbolOperand(const std::string& str) : std::string(str) {}
};

using immediateOperand = int64_t;

class registerOperand
{
  public:
    registerType type = registerType::UNDEFINED;
    uint32_t reg_id = 0;
};

class sprRegisterOperand
{
  public:
    sprType type = sprType::UNDEFINED;
};

class addressOperand
{
  public:
    registerOperand reg = registerOperand();
    symbolOperand symbol = symbolOperand();
    immediateOperand imm = 0;
};

enum class FuncType : uint8_t
{
    Undefined,
    Entry,
    Func
};

enum class FuncAttr : uint8_t
{
    Visible
};

struct FunctionParameter
{
    std::string name;
    Ptx::dataType type;
    uint8_t id;
};

template <dataType T>
struct PtxNativeType;
template <>
struct PtxNativeType<dataType::Pred>
{
    using type = bool;
};
template <>
struct PtxNativeType<dataType::U8>
{
    using type = uint8_t;
};
template <>
struct PtxNativeType<dataType::U16>
{
    using type = uint16_t;
};
template <>
struct PtxNativeType<dataType::U32>
{
    using type = uint32_t;
};
template <>
struct PtxNativeType<dataType::U64>
{
    using type = uint64_t;
};
template <>
struct PtxNativeType<dataType::S8>
{
    using type = int8_t;
};
template <>
struct PtxNativeType<dataType::S16>
{
    using type = int16_t;
};
template <>
struct PtxNativeType<dataType::S32>
{
    using type = int32_t;
};
template <>
struct PtxNativeType<dataType::S64>
{
    using type = int64_t;
};
template <>
struct PtxNativeType<dataType::F16>
{
    using type = _Float16;
};
template <>
struct PtxNativeType<dataType::F32>
{
    using type = float;
};
template <>
struct PtxNativeType<dataType::F64>
{
    using type = double;
};
template <>
struct PtxNativeType<dataType::B8>
{
    using type = uint8_t;
};
template <>
struct PtxNativeType<dataType::B16>
{
    using type = uint16_t;
};
template <>
struct PtxNativeType<dataType::B32>
{
    using type = uint32_t;
};
template <>
struct PtxNativeType<dataType::B64>
{
    using type = uint64_t;
};
template <>
struct PtxNativeType<dataType::UNDEFINED>
{
    using type = int;
};

template <dataType T>
using ptx_native_t = typename PtxNativeType<T>::type;

} // namespace Ptx
} // namespace Emulator

// NOLINTEND