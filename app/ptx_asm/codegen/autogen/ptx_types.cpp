#include "ptx_types.h"
#include "utils.h"

#include <regex>
#include <iostream>
#include <cstring>

// NOLINTBEGIN

namespace Emulator {
namespace Ptx {




static std::unordered_map<std::string, pragmavalQl> StrTopragmavalQl {
    { "", pragmavalQl::UNDEFINED },
    { "nounroll", pragmavalQl::Nounroll },
    { "used_bytes_mask", pragmavalQl::Used_bytes_mask },
};

template<>
pragmavalQl FromString<pragmavalQl>(const std::string& str) {
    return StrTopragmavalQl[str];
}




static std::unordered_map<std::string, localAbsDataType> StrTolocalAbsDataType {
    { "", localAbsDataType::UNDEFINED },
    { "s32", localAbsDataType::S32 },
    { "s64", localAbsDataType::S64 },
    { "f32", localAbsDataType::F32 },
    { "f64", localAbsDataType::F64 },
};

template<>
localAbsDataType FromString<localAbsDataType>(const std::string& str) {
    return StrTolocalAbsDataType[str];
}




static std::unordered_map<std::string, cvtaspaceQl> StrTocvtaspaceQl {
    { "", cvtaspaceQl::UNDEFINED },
    { "const", cvtaspaceQl::Const },
    { "global", cvtaspaceQl::Global },
    { "local", cvtaspaceQl::Local },
    { "param", cvtaspaceQl::Param },
    { "param::entry", cvtaspaceQl::ParamEntry },
    { "shared", cvtaspaceQl::Shared },
    { "shared::cta", cvtaspaceQl::SharedCta },
    { "shared::cluster", cvtaspaceQl::SharedCluster },
};

template<>
cvtaspaceQl FromString<cvtaspaceQl>(const std::string& str) {
    return StrTocvtaspaceQl[str];
}




static std::unordered_map<std::string, localCvtaDataType> StrTolocalCvtaDataType {
    { "", localCvtaDataType::UNDEFINED },
    { "u32", localCvtaDataType::U32 },
    { "u64", localCvtaDataType::U64 },
};

template<>
localCvtaDataType FromString<localCvtaDataType>(const std::string& str) {
    return StrTolocalCvtaDataType[str];
}




static std::unordered_map<std::string, setpcmpQl> StrTosetpcmpQl {
    { "", setpcmpQl::UNDEFINED },
    { "eq", setpcmpQl::Eq },
    { "ne", setpcmpQl::Ne },
    { "lt", setpcmpQl::Lt },
    { "le", setpcmpQl::Le },
    { "gt", setpcmpQl::Gt },
    { "ge", setpcmpQl::Ge },
    { "lo", setpcmpQl::Lo },
    { "ls", setpcmpQl::Ls },
    { "hi", setpcmpQl::Hi },
    { "hs", setpcmpQl::Hs },
    { "equ", setpcmpQl::Equ },
    { "neu", setpcmpQl::Neu },
    { "ltu", setpcmpQl::Ltu },
    { "leu", setpcmpQl::Leu },
    { "gtu", setpcmpQl::Gtu },
    { "geu", setpcmpQl::Geu },
    { "num", setpcmpQl::Num },
    { "nan", setpcmpQl::Nan },
};

template<>
setpcmpQl FromString<setpcmpQl>(const std::string& str) {
    return StrTosetpcmpQl[str];
}




static std::unordered_map<std::string, setpboolQl> StrTosetpboolQl {
    { "", setpboolQl::UNDEFINED },
    { "and", setpboolQl::And },
    { "or", setpboolQl::Or },
    { "xor", setpboolQl::Xor },
};

template<>
setpboolQl FromString<setpboolQl>(const std::string& str) {
    return StrTosetpboolQl[str];
}




static std::unordered_map<std::string, localSetpDataType> StrTolocalSetpDataType {
    { "", localSetpDataType::UNDEFINED },
    { "b16", localSetpDataType::B16 },
    { "b32", localSetpDataType::B32 },
    { "b64", localSetpDataType::B64 },
    { "u16", localSetpDataType::U16 },
    { "u32", localSetpDataType::U32 },
    { "u64", localSetpDataType::U64 },
    { "s16", localSetpDataType::S16 },
    { "s32", localSetpDataType::S32 },
    { "s64", localSetpDataType::S64 },
    { "f32", localSetpDataType::F32 },
    { "f64", localSetpDataType::F64 },
};

template<>
localSetpDataType FromString<localSetpDataType>(const std::string& str) {
    return StrTolocalSetpDataType[str];
}




static std::unordered_map<std::string, localAddDataType> StrTolocalAddDataType {
    { "", localAddDataType::UNDEFINED },
    { "u16", localAddDataType::U16 },
    { "u32", localAddDataType::U32 },
    { "u64", localAddDataType::U64 },
    { "s16", localAddDataType::S16 },
    { "s32", localAddDataType::S32 },
    { "s64", localAddDataType::S64 },
    { "f32", localAddDataType::F32 },
    { "f64", localAddDataType::F64 },
};

template<>
localAddDataType FromString<localAddDataType>(const std::string& str) {
    return StrTolocalAddDataType[str];
}




static std::unordered_map<std::string, localMovDataType> StrTolocalMovDataType {
    { "", localMovDataType::UNDEFINED },
    { "pred", localMovDataType::Pred },
    { "b16", localMovDataType::B16 },
    { "b32", localMovDataType::B32 },
    { "b64", localMovDataType::B64 },
    { "u16", localMovDataType::U16 },
    { "u32", localMovDataType::U32 },
    { "u64", localMovDataType::U64 },
    { "s16", localMovDataType::S16 },
    { "s32", localMovDataType::S32 },
    { "s64", localMovDataType::S64 },
    { "f32", localMovDataType::F32 },
    { "f64", localMovDataType::F64 },
};

template<>
localMovDataType FromString<localMovDataType>(const std::string& str) {
    return StrTolocalMovDataType[str];
}




static std::unordered_map<std::string, localShlDataType> StrTolocalShlDataType {
    { "", localShlDataType::UNDEFINED },
    { "b16", localShlDataType::B16 },
    { "b32", localShlDataType::B32 },
    { "b64", localShlDataType::B64 },
};

template<>
localShlDataType FromString<localShlDataType>(const std::string& str) {
    return StrTolocalShlDataType[str];
}




static std::unordered_map<std::string, localAndDataType> StrTolocalAndDataType {
    { "", localAndDataType::UNDEFINED },
    { "pred", localAndDataType::Pred },
    { "b16", localAndDataType::B16 },
    { "b32", localAndDataType::B32 },
    { "b64", localAndDataType::B64 },
};

template<>
localAndDataType FromString<localAndDataType>(const std::string& str) {
    return StrTolocalAndDataType[str];
}




static std::unordered_map<std::string, mulmodeQl> StrTomulmodeQl {
    { "", mulmodeQl::UNDEFINED },
    { "hi", mulmodeQl::Hi },
    { "lo", mulmodeQl::Lo },
    { "wide", mulmodeQl::Wide },
};

template<>
mulmodeQl FromString<mulmodeQl>(const std::string& str) {
    return StrTomulmodeQl[str];
}




static std::unordered_map<std::string, localMulDataType> StrTolocalMulDataType {
    { "", localMulDataType::UNDEFINED },
    { "f16", localMulDataType::F16 },
    { "f32", localMulDataType::F32 },
    { "f64", localMulDataType::F64 },
    { "u16", localMulDataType::U16 },
    { "u32", localMulDataType::U32 },
    { "u64", localMulDataType::U64 },
    { "s16", localMulDataType::S16 },
    { "s32", localMulDataType::S32 },
    { "s64", localMulDataType::S64 },
};

template<>
localMulDataType FromString<localMulDataType>(const std::string& str) {
    return StrTolocalMulDataType[str];
}




static std::unordered_map<std::string, localEx2DataType> StrTolocalEx2DataType {
    { "", localEx2DataType::UNDEFINED },
    { "f16", localEx2DataType::F16 },
    { "f32", localEx2DataType::F32 },
    { "f64", localEx2DataType::F64 },
};

template<>
localEx2DataType FromString<localEx2DataType>(const std::string& str) {
    return StrTolocalEx2DataType[str];
}




static std::unordered_map<std::string, barmodeQl> StrTobarmodeQl {
    { "", barmodeQl::UNDEFINED },
    { "sync", barmodeQl::Sync },
    { "arrive", barmodeQl::Arrive },
};

template<>
barmodeQl FromString<barmodeQl>(const std::string& str) {
    return StrTobarmodeQl[str];
}




static std::unordered_map<std::string, stspaceQl> StrTostspaceQl {
    { "", stspaceQl::UNDEFINED },
    { "const", stspaceQl::Const },
    { "global", stspaceQl::Global },
    { "local", stspaceQl::Local },
    { "param", stspaceQl::Param },
    { "param::entry", stspaceQl::ParamEntry },
    { "param::func", stspaceQl::ParamFunc },
    { "shared", stspaceQl::Shared },
    { "shared::cta", stspaceQl::SharedCta },
    { "shared::cluster", stspaceQl::SharedCluster },
};

template<>
stspaceQl FromString<stspaceQl>(const std::string& str) {
    return StrTostspaceQl[str];
}




static std::unordered_map<std::string, localStDataType> StrTolocalStDataType {
    { "", localStDataType::UNDEFINED },
    { "b8", localStDataType::B8 },
    { "b16", localStDataType::B16 },
    { "b32", localStDataType::B32 },
    { "b64", localStDataType::B64 },
    { "u8", localStDataType::U8 },
    { "u16", localStDataType::U16 },
    { "u32", localStDataType::U32 },
    { "u64", localStDataType::U64 },
    { "s8", localStDataType::S8 },
    { "s16", localStDataType::S16 },
    { "s32", localStDataType::S32 },
    { "s64", localStDataType::S64 },
    { "f32", localStDataType::F32 },
    { "f64", localStDataType::F64 },
};

template<>
localStDataType FromString<localStDataType>(const std::string& str) {
    return StrTolocalStDataType[str];
}




static std::unordered_map<std::string, fmamodeQl> StrTofmamodeQl {
    { "", fmamodeQl::UNDEFINED },
    { "rn", fmamodeQl::Rn },
    { "rz", fmamodeQl::Rz },
    { "rm", fmamodeQl::Rm },
    { "rp", fmamodeQl::Rp },
};

template<>
fmamodeQl FromString<fmamodeQl>(const std::string& str) {
    return StrTofmamodeQl[str];
}




static std::unordered_map<std::string, localFmaDataType> StrTolocalFmaDataType {
    { "", localFmaDataType::UNDEFINED },
    { "f32", localFmaDataType::F32 },
    { "f64", localFmaDataType::F64 },
};

template<>
localFmaDataType FromString<localFmaDataType>(const std::string& str) {
    return StrTolocalFmaDataType[str];
}




static std::unordered_map<std::string, localNegDataType> StrTolocalNegDataType {
    { "", localNegDataType::UNDEFINED },
    { "s16", localNegDataType::S16 },
    { "s32", localNegDataType::S32 },
    { "s64", localNegDataType::S64 },
};

template<>
localNegDataType FromString<localNegDataType>(const std::string& str) {
    return StrTolocalNegDataType[str];
}




static std::unordered_map<std::string, localSubDataType> StrTolocalSubDataType {
    { "", localSubDataType::UNDEFINED },
    { "f32", localSubDataType::F32 },
    { "s64", localSubDataType::S64 },
    { "f64", localSubDataType::F64 },
};

template<>
localSubDataType FromString<localSubDataType>(const std::string& str) {
    return StrTolocalSubDataType[str];
}




static std::unordered_map<std::string, ldspaceQl> StrToldspaceQl {
    { "", ldspaceQl::UNDEFINED },
    { "const", ldspaceQl::Const },
    { "global", ldspaceQl::Global },
    { "local", ldspaceQl::Local },
    { "param", ldspaceQl::Param },
    { "param::entry", ldspaceQl::ParamEntry },
    { "param::func", ldspaceQl::ParamFunc },
    { "shared", ldspaceQl::Shared },
    { "shared::cta", ldspaceQl::SharedCta },
    { "shared::cluster", ldspaceQl::SharedCluster },
};

template<>
ldspaceQl FromString<ldspaceQl>(const std::string& str) {
    return StrToldspaceQl[str];
}




static std::unordered_map<std::string, localLdDataType> StrTolocalLdDataType {
    { "", localLdDataType::UNDEFINED },
    { "b8", localLdDataType::B8 },
    { "b16", localLdDataType::B16 },
    { "b32", localLdDataType::B32 },
    { "b64", localLdDataType::B64 },
    { "u8", localLdDataType::U8 },
    { "u16", localLdDataType::U16 },
    { "u32", localLdDataType::U32 },
    { "u64", localLdDataType::U64 },
    { "s8", localLdDataType::S8 },
    { "s16", localLdDataType::S16 },
    { "s32", localLdDataType::S32 },
    { "s64", localLdDataType::S64 },
    { "f32", localLdDataType::F32 },
    { "f64", localLdDataType::F64 },
};

template<>
localLdDataType FromString<localLdDataType>(const std::string& str) {
    return StrTolocalLdDataType[str];
}




static std::unordered_map<std::string, localRcpDataType> StrTolocalRcpDataType {
    { "", localRcpDataType::UNDEFINED },
    { "f16", localRcpDataType::F16 },
    { "f32", localRcpDataType::F32 },
    { "f64", localRcpDataType::F64 },
};

template<>
localRcpDataType FromString<localRcpDataType>(const std::string& str) {
    return StrTolocalRcpDataType[str];
}




static std::unordered_map<std::string, localTanhDataType> StrTolocalTanhDataType {
    { "", localTanhDataType::UNDEFINED },
    { "f32", localTanhDataType::F32 },
    { "f64", localTanhDataType::F64 },
};

template<>
localTanhDataType FromString<localTanhDataType>(const std::string& str) {
    return StrTolocalTanhDataType[str];
}




static std::unordered_map<std::string, madmodeQl> StrTomadmodeQl {
    { "", madmodeQl::UNDEFINED },
    { "hi", madmodeQl::Hi },
    { "lo", madmodeQl::Lo },
    { "wide", madmodeQl::Wide },
};

template<>
madmodeQl FromString<madmodeQl>(const std::string& str) {
    return StrTomadmodeQl[str];
}




static std::unordered_map<std::string, localMadDataType> StrTolocalMadDataType {
    { "", localMadDataType::UNDEFINED },
    { "u16", localMadDataType::U16 },
    { "u32", localMadDataType::U32 },
    { "u64", localMadDataType::U64 },
    { "s16", localMadDataType::S16 },
    { "s32", localMadDataType::S32 },
    { "s64", localMadDataType::S64 },
};

template<>
localMadDataType FromString<localMadDataType>(const std::string& str) {
    return StrTolocalMadDataType[str];
}




static std::unordered_map<std::string, localCopysignDataType> StrTolocalCopysignDataType {
    { "", localCopysignDataType::UNDEFINED },
    { "f32", localCopysignDataType::F32 },
    { "f64", localCopysignDataType::F64 },
};

template<>
localCopysignDataType FromString<localCopysignDataType>(const std::string& str) {
    return StrTolocalCopysignDataType[str];
}




static std::unordered_map<std::string, localSelpDataType> StrTolocalSelpDataType {
    { "", localSelpDataType::UNDEFINED },
    { "b16", localSelpDataType::B16 },
    { "b32", localSelpDataType::B32 },
    { "b64", localSelpDataType::B64 },
    { "u16", localSelpDataType::U16 },
    { "u32", localSelpDataType::U32 },
    { "u64", localSelpDataType::U64 },
    { "s16", localSelpDataType::S16 },
    { "s32", localSelpDataType::S32 },
    { "s64", localSelpDataType::S64 },
    { "f32", localSelpDataType::F32 },
    { "f64", localSelpDataType::F64 },
};

template<>
localSelpDataType FromString<localSelpDataType>(const std::string& str) {
    return StrTolocalSelpDataType[str];
}




static std::unordered_map<std::string, cvtmodeQl> StrTocvtmodeQl {
    { "", cvtmodeQl::UNDEFINED },
    { "rtz", cvtmodeQl::Rtz },
    { "rm", cvtmodeQl::Rm },
    { "rz", cvtmodeQl::Rz },
    { "rn", cvtmodeQl::Rn },
    { "rp", cvtmodeQl::Rp },
};

template<>
cvtmodeQl FromString<cvtmodeQl>(const std::string& str) {
    return StrTocvtmodeQl[str];
}




static std::unordered_map<std::string, localCvtSrcDataType> StrTolocalCvtSrcDataType {
    { "", localCvtSrcDataType::UNDEFINED },
    { "u8", localCvtSrcDataType::U8 },
    { "u16", localCvtSrcDataType::U16 },
    { "u32", localCvtSrcDataType::U32 },
    { "u64", localCvtSrcDataType::U64 },
    { "s8", localCvtSrcDataType::S8 },
    { "s16", localCvtSrcDataType::S16 },
    { "s32", localCvtSrcDataType::S32 },
    { "s64", localCvtSrcDataType::S64 },
    { "f16", localCvtSrcDataType::F16 },
    { "f32", localCvtSrcDataType::F32 },
    { "f64", localCvtSrcDataType::F64 },
};

template<>
localCvtSrcDataType FromString<localCvtSrcDataType>(const std::string& str) {
    return StrTolocalCvtSrcDataType[str];
}




static std::unordered_map<std::string, localCvtDstDataType> StrTolocalCvtDstDataType {
    { "", localCvtDstDataType::UNDEFINED },
    { "u8", localCvtDstDataType::U8 },
    { "u16", localCvtDstDataType::U16 },
    { "u32", localCvtDstDataType::U32 },
    { "u64", localCvtDstDataType::U64 },
    { "s8", localCvtDstDataType::S8 },
    { "s16", localCvtDstDataType::S16 },
    { "s32", localCvtDstDataType::S32 },
    { "s64", localCvtDstDataType::S64 },
    { "f16", localCvtDstDataType::F16 },
    { "f32", localCvtDstDataType::F32 },
    { "f64", localCvtDstDataType::F64 },
};

template<>
localCvtDstDataType FromString<localCvtDstDataType>(const std::string& str) {
    return StrTolocalCvtDstDataType[str];
}





static std::unordered_map<std::string, dataType> StrTodataType {
    { "", dataType::UNDEFINED },
    { "f16", dataType::F16 },
    { "f32", dataType::F32 },
    { "f64", dataType::F64 },
    { "s8", dataType::S8 },
    { "s16", dataType::S16 },
    { "s32", dataType::S32 },
    { "s64", dataType::S64 },
    { "u8", dataType::U8 },
    { "u16", dataType::U16 },
    { "u32", dataType::U32 },
    { "u64", dataType::U64 },
    { "b8", dataType::B8 },
    { "b16", dataType::B16 },
    { "b32", dataType::B32 },
    { "b64", dataType::B64 },
    { "pred", dataType::Pred },
};

template<>
dataType FromString<dataType>(const std::string& str) {
    return StrTodataType[str];
}



static std::unordered_map<std::string, registerType> StrToregisterType {
    { "", registerType::UNDEFINED },
    { "f", registerType::F },
    { "fd", registerType::Fd },
    { "p", registerType::P },
    { "r", registerType::R },
    { "rd", registerType::Rd },
    { "spr", registerType::Spr },
};

template<>
registerType FromString<registerType>(const std::string& str) {
    return StrToregisterType[str];
}



static std::unordered_map<std::string, sprType> StrTosprType {
    { "", sprType::UNDEFINED },
    { "tid.x", sprType::TidX },
    { "tid.y", sprType::TidY },
    { "tid.z", sprType::TidZ },
    { "ctaid.x", sprType::CtaidX },
    { "ctaid.y", sprType::CtaidY },
    { "ctaid.z", sprType::CtaidZ },
    { "ntid.x", sprType::NtidX },
    { "ntid.y", sprType::NtidY },
    { "ntid.z", sprType::NtidZ },
};

template<>
sprType FromString<sprType>(const std::string& str) {
    return StrTosprType[str];
}


template <>
symbolOperand FromString(const std::string& str) {
    return symbolOperand(str);
}

template<>
int32_t FromString(const std::string& str) {
    return static_cast<int32_t>(std::strtol(str.c_str(), nullptr, 10));
}

template<>
int64_t FromString(const std::string& str) {
    // PTX hex-float immediates: 0f<8 hex digits> = 32-bit IEEE 754 bit pattern
    if (str.size() >= 2 && str[0] == '0' && (str[1] == 'f' || str[1] == 'F')) {
        uint32_t bits = static_cast<uint32_t>(std::strtoul(str.c_str() + 2, nullptr, 16));
        return static_cast<int64_t>(static_cast<uint64_t>(bits));
    }
    // PTX hex-double immediates: 0d<16 hex digits> = 64-bit IEEE 754 bit pattern
    if (str.size() >= 2 && str[0] == '0' && (str[1] == 'd' || str[1] == 'D')) {
        uint64_t bits = std::strtoull(str.c_str() + 2, nullptr, 16);
        return static_cast<int64_t>(bits);
    }
    // 0x hex integer or plain decimal
    return std::strtoll(str.c_str(), nullptr, 0);
}

template<>
uint32_t FromString(const std::string& str) {
    return static_cast<uint32_t>(std::strtoul(str.c_str(), nullptr, 0));
}

template<>
uint64_t FromString(const std::string& str) {
    return std::strtoull(str.c_str(), nullptr, 0);
}

template<>
bool FromString(const std::string& str) {
    return static_cast<bool>(str.size());
}

template <>
registerOperand FromString(const std::string& str) {
    constexpr std::string_view pattern = "%([a-z]+)([0-9]*),?";
    static std::regex re(pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto res = registerOperand();
    if (std::regex_match(str, match, re)) {
            if (match[1].matched) res.type = FromString<registerType>(match[1].str());
            if (match[2].matched) res.reg_id = FromString<uint32_t>(match[2].str());
    } else {
        throw std::runtime_error("Register operand is not matched [" + str + "]");
    }
    return res;
}

template <>
sprRegisterOperand FromString(const std::string& str) {
    auto res = sprRegisterOperand();
    std::string stripped = (!str.empty() && str[0] == '%') ? str.substr(1) : str;
    res.type = FromString<sprType>(stripped);
    return res;
}

template <>
addressOperand FromString(const std::string& str) {
    constexpr std::string_view pattern = "^\\[(%[a-z]+[0-9]*)?([A-z0-9_]+)?\\+?(0?x?[\\-A-Fa-f0-9]+)?\\]$";
    static std::regex re(pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto res = addressOperand();
    if (std::regex_match(str, match, re)) {
            if (match[1].matched) res.reg = FromString<registerOperand>(match[1].str());
            if (match[2].matched) res.symbol = FromString<symbolOperand>(match[2].str());
            if (match[3].matched) res.imm = FromString<immediateOperand>(match[3].str());
    } else {
        throw std::runtime_error("Address operand is not mutched.");
    }
    return res;
}

const std::unordered_map<std::string, FuncType> StrToFuncType{
    {"", FuncType::Undefined},
    {"entry", FuncType::Entry},
    {"func", FuncType::Func},
};

const std::unordered_map<std::string, FuncAttr> StrToFuncAttr{
    {"visible", FuncAttr::Visible},
};

template <>
FuncType FromString(const std::string& str)
{
    return StrToFuncType.at(str);
}

template <>
FuncAttr FromString(const std::string& str)
{
    return StrToFuncAttr.at(str);
}

} // Ptx
} // Emulator

// NOLINTEND