#pragma once

#include <cmath>
#include <cstring>
#include <type_traits>

namespace Emulator {
namespace Ptx {

namespace {

// Read a typed value from a uint64_t register slot.
// Integers: static_cast (preserves 2's-complement bit pattern).
// Floats / float16: memcpy (avoids type-punning UB).
template<typename T>
T reg_cast(uint64_t raw)
{
    if constexpr (std::is_same_v<T, bool>)
    {
        return raw != 0;
    }
    else if constexpr (std::is_integral_v<T>)
    {
        return static_cast<T>(raw);
    }
    else
    {
        T val;
        std::memcpy(&val, &raw, sizeof(T));
        return val;
    }
}

// Write a typed value back into a uint64_t register slot.
// Unsigned integers: zero-extend naturally.
// Signed integers: cast through the matching unsigned type so the bit pattern
//   of the value is preserved in the low bytes (matches how reg_cast reads it).
// Floats: memcpy into the low bytes; upper bytes stay 0.
template<typename T>
uint64_t to_u64(T val)
{
    if constexpr (std::is_same_v<T, bool>)
    {
        return val ? 1ULL : 0ULL;
    }
    else if constexpr (std::is_integral_v<T>)
    {
        return static_cast<uint64_t>(static_cast<std::make_unsigned_t<T>>(val));
    }
    else
    {
        uint64_t raw = 0;
        std::memcpy(&raw, &val, sizeof(T));
        return raw;
    }
}

// Runtime byte size of a single PTX data type element.
static size_t ptx_sizeof(dataType dt)
{
    switch (dt)
    {
        case dataType::B8:  case dataType::U8:  case dataType::S8:  return 1;
        case dataType::B16: case dataType::U16: case dataType::S16: case dataType::F16: return 2;
        case dataType::B32: case dataType::U32: case dataType::S32: case dataType::F32: return 4;
        case dataType::B64: case dataType::U64: case dataType::S64: case dataType::F64: return 8;
        default: return 1;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// reg — allocate register file slots for this thread
// ---------------------------------------------------------------------------
void regInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    wc->thread_regs[lid][reg_] = RegisterContext(count_);
}

// ---------------------------------------------------------------------------
// shared — register a named symbol in the block's shared memory
// ---------------------------------------------------------------------------
void sharedInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)
{
    size_t byte_size = ptx_sizeof(data_) * count_;
    wc->registerSharedSymbol(symbol_, byte_size, align_);
}

// ---------------------------------------------------------------------------
// pragma — compiler hint, no runtime effect
// ---------------------------------------------------------------------------
void pragmaInstruction::ExecuteWarp(std::shared_ptr<WarpContext>& wc) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)wc;
}

// ---------------------------------------------------------------------------
// cvta — address-space conversion
// In this flat-memory emulator all address spaces share the host address
// space, so cvta is an identity copy of the pointer value.
// ---------------------------------------------------------------------------
template<dataType Data>
void cvtaInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    uint64_t val = wc->thread_regs[lid][src_.type][src_.reg_id];
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = val;
}

// ---------------------------------------------------------------------------
// setp — set predicate from comparison
// ---------------------------------------------------------------------------
template<dataType Data>
void setpInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));

    bool result = false;
    switch (cmp_)
    {
        case setpcmpQl::Eq:
        case setpcmpQl::Equ: result = (s1 == s2); break;
        case setpcmpQl::Ne:
        case setpcmpQl::Neu: result = (s1 != s2); break;
        case setpcmpQl::Lt:
        case setpcmpQl::Ltu:
        case setpcmpQl::Lo:  result = (s1 < s2);  break;
        case setpcmpQl::Le:
        case setpcmpQl::Leu:
        case setpcmpQl::Ls:  result = (s1 <= s2); break;
        case setpcmpQl::Gt:
        case setpcmpQl::Gtu:
        case setpcmpQl::Hi:  result = (s1 > s2);  break;
        case setpcmpQl::Ge:
        case setpcmpQl::Geu:
        case setpcmpQl::Hs:  result = (s1 >= s2); break;
        case setpcmpQl::Num: result = (s1 == s1) && (s2 == s2); break; // neither NaN
        case setpcmpQl::Nan: result = (s1 != s1) || (s2 != s2); break; // at least one NaN
        default: result = false; break;
    }

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = result ? 1U : 0U;
}


// ---------------------------------------------------------------------------
// copysign — PTX: copysign d, a, b → magnitude from b (src2), sign from a (src1)
// std::copysign(magnitude, sign) → std::copysign(s2, s1)
// ---------------------------------------------------------------------------
template<dataType Data>
void copysignInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(std::copysign(s2, s1));
}

// ---------------------------------------------------------------------------
// selp — ternary select on predicate  (types: b16/b32/b64, u*, s*, f*)
//   dst = (src3 != 0) ? src1|imm1 : src2|imm2
// ---------------------------------------------------------------------------
template<dataType Data>
void selpInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T a = (src1_.type != registerType::UNDEFINED)
              ? reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id])
              : reg_cast<T>(static_cast<uint64_t>(imm1_));
    T b = (src2_.type != registerType::UNDEFINED)
              ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
              : reg_cast<T>(static_cast<uint64_t>(imm2_));
    bool cond = wc->thread_regs[lid][src3_.type][src3_.reg_id] != 0;

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(cond ? a : b);
}


// ---------------------------------------------------------------------------
// add — integer addition
// ---------------------------------------------------------------------------
template<dataType Data>
void addInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(s1 + s2));
}

// ---------------------------------------------------------------------------
// mov — move register / special register / immediate
// ---------------------------------------------------------------------------
template<dataType Data>
void movInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    uint64_t val = 0;
    if (src_.type != registerType::UNDEFINED)
    {
        val = wc->thread_regs[lid][src_.type][src_.reg_id];
    }
    else if (spr_.type != sprType::UNDEFINED)
    {
        val = wc->spr_regs[lid][spr_.type];
    }
    else if (!symbol_.empty())
    {
        val = reinterpret_cast<uint64_t>(wc->getSharedPtr(symbol_));
    }
    else
    {
        val = to_u64<T>(reg_cast<T>(static_cast<uint64_t>(imm_)));
    }

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = val;
}

// ---------------------------------------------------------------------------
// shl — logical shift left (shift amount always in imm_)
// ---------------------------------------------------------------------------
template<dataType Data>
void shlInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T val   = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    auto sh = static_cast<uint32_t>(imm_);

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(val << sh));
}

// ---------------------------------------------------------------------------
// abs — absolute value
// ---------------------------------------------------------------------------
template<dataType Data>
void absInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T val   = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    auto res = std::abs(val);

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(res);
}

// ---------------------------------------------------------------------------
// and — bitwise AND  (types: pred, b16, b32, b64)
// ---------------------------------------------------------------------------
template<dataType Data>
void andInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(s1 & s2));
}

// ---------------------------------------------------------------------------
// mul — multiply  (modes: lo / hi / wide)
// ---------------------------------------------------------------------------
template<dataType Data>
void mulInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));

    uint64_t result = 0;
    switch (mode_)
    {
        case mulmodeQl::Lo:
            result = to_u64<T>(T(s1 * s2));
            break;
        case mulmodeQl::Hi:
        {
            using W = std::conditional_t<std::is_signed_v<T>, __int128, unsigned __int128>;
            W wide  = W(s1) * W(s2);
            result  = to_u64<T>(static_cast<T>(wide >> (sizeof(T) * 8)));
            break;
        }
        case mulmodeQl::Wide:
        {
            // Widen: 16→32, 32→64 (result fits in uint64_t)
            using W = std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>;
            result = static_cast<uint64_t>(W(s1) * W(s2));
            break;
        }
        default:
            result = to_u64<T>(T(s1 * s2));
    }
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = result;
}


// ---------------------------------------------------------------------------
// ex2 — base-2 exponential  (dst = 2^src,  types: f16, f32, f64)
// ftz_ (flush-to-zero) is a hardware hint; ignored in the emulator.
// ---------------------------------------------------------------------------
template<dataType Data>
void ex2Instruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T src = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(static_cast<T>(std::exp2(static_cast<double>(src))));
}

// ---------------------------------------------------------------------------
// rcp — reciprocal  (dst = 1/src,  types: f16, f32, f64)
// ftz_ is ignored in the emulator.
// ---------------------------------------------------------------------------
template<dataType Data>
void rcpInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T src = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(1) / src);
}

// ---------------------------------------------------------------------------
// tanh — hyperbolic tangent  (dst = tanh(src),  types: f32, f64)
// PTX: tanh.approx[.ftz].f32 dst, src
// ftz_ is ignored in the emulator.
// ---------------------------------------------------------------------------
template<dataType Data>
void tanhInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T src = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(static_cast<T>(std::tanh(static_cast<double>(src))));
}


// ---------------------------------------------------------------------------
// fma — fused multiply-add  (dst = src1 * src2 + src3,  types: f32 / f64)
// src2 may be a register (src2_) or immediate (imm1_).
// src3 may be a register (src3_) or immediate (imm2_).
// ---------------------------------------------------------------------------
template<dataType Data>
void fmaInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm1_));
    T s3 = (src3_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src3_.type][src3_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm2_));

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(std::fma(s1, s2, s3));
}

// ---------------------------------------------------------------------------
// st — store to memory  (addr = reg + imm offset)
// ---------------------------------------------------------------------------
template<dataType Data>
void stInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    uintptr_t addr = 0;
    if (addr_.reg.type != registerType::UNDEFINED)
    {
        addr = static_cast<uintptr_t>(wc->thread_regs[lid][addr_.reg.type][addr_.reg.reg_id]);
    }
    addr += static_cast<ptrdiff_t>(addr_.imm);

    T val = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    std::memcpy(reinterpret_cast<void*>(addr), &val, sizeof(T));
}

// ---------------------------------------------------------------------------
// neg — arithmetic negation  (types: s16, s32, s64)
// ---------------------------------------------------------------------------
template<dataType Data>
void negInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T val = (src_.type != registerType::UNDEFINED)
                ? reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id])
                : reg_cast<T>(static_cast<uint64_t>(imm_));

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(-val));
}

// ---------------------------------------------------------------------------
// sub — subtraction  (types: f32, f64)
// ---------------------------------------------------------------------------
template<dataType Data>
void subInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(s1 - s2));
}

// ---------------------------------------------------------------------------
// bra — conditional / unconditional branch
// When no predicate register is specified the branch is unconditional.
// The .uni flag (uni_) only affects divergence semantics; the sequential
// emulator ignores it.
// ---------------------------------------------------------------------------
void braInstruction::ExecuteBranch(std::shared_ptr<WarpContext>& wc)
{
    if (prd_.type == registerType::UNDEFINED)
    {
        wc->gotoBasicBlock(sym_);
        return;
    }

    auto mask        = wc->GetPredicateMask(prd_.reg_id);
    uint64_t branch_mask = mask & wc->execution_mask;
    if (branch_mask == wc->execution_mask)
    {
        wc->gotoBasicBlock(sym_);
    }
    else if (branch_mask == 0)
    {
        wc->pc += 1;
    }
    else
    {
        uint32_t fall_through_mask = wc->execution_mask & ~(uint32_t)branch_mask;
        wc->execution_mask = (uint32_t)branch_mask;
        wc->execution_stack.push({wc->pc + 1, fall_through_mask});
        wc->gotoBasicBlock(sym_);
    }
}

// ---------------------------------------------------------------------------
// label — label marker, no runtime effect
// ---------------------------------------------------------------------------
void labelInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)lid;
    (void)wc;
}

// ---------------------------------------------------------------------------
// ld — load from memory
//   Register address:  addr = reg + imm
//   Symbol address:    addr = getParamPtr(symbol) + imm  (kernel params)
// ---------------------------------------------------------------------------
template<dataType Data>
void ldInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    uintptr_t addr = 0;
    if (addr_.reg.type != registerType::UNDEFINED)
    {
        addr = static_cast<uintptr_t>(wc->thread_regs[lid][addr_.reg.type][addr_.reg.reg_id]);
        addr += static_cast<ptrdiff_t>(addr_.imm);
    }
    else if (!addr_.symbol.empty())
    {
        addr = reinterpret_cast<uintptr_t>(wc->getParamPtr(addr_.symbol));
        addr += static_cast<ptrdiff_t>(addr_.imm);
    }

    T val = T{};
    std::memcpy(&val, reinterpret_cast<const void*>(addr), sizeof(T));
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(val);
}

// ---------------------------------------------------------------------------
// mad — multiply-add  (dst = a*b + c,  modes: lo / hi / wide)
// ---------------------------------------------------------------------------
template<dataType Data>
void madInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);
    T s3 = reg_cast<T>(wc->thread_regs[lid][src3_.type][src3_.reg_id]);

    uint64_t result = 0;
    switch (mode_)
    {
        case madmodeQl::Lo:
            result = to_u64<T>(T(s1 * s2 + s3));
            break;
        case madmodeQl::Hi:
        {
            using W = std::conditional_t<std::is_signed_v<T>, __int128, unsigned __int128>;
            W wide  = W(s1) * W(s2);
            T hi    = static_cast<T>(wide >> (sizeof(T) * 8));
            result  = to_u64<T>(T(hi + s3));
            break;
        }
        case madmodeQl::Wide:
        {
            using W = std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>;
            result = static_cast<uint64_t>(W(s1) * W(s2) + W(s3));
            break;
        }
        default:
            result = to_u64<T>(T(s1 * s2 + s3));
    }
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = result;
}

// ---------------------------------------------------------------------------
// ret — return / pop execution stack
// ---------------------------------------------------------------------------
void retInstruction::ExecuteBranch(std::shared_ptr<WarpContext>& wc) // NOLINT(readability-convert-member-functions-to-static)
{
    if (wc->execution_stack.empty())
    {
        wc->pc = WarpContext::EOC;
    }
    else
    {
        auto [pc, mask] = wc->execution_stack.top();
        wc->pc          = pc;
        wc->execution_mask = mask;
        wc->execution_stack.pop();
    }
}

// ---------------------------------------------------------------------------
// cvt — convert between numeric types
//
// PTX syntax: cvt.dst_type.src_type  dst, src
// The generated code captures the FIRST mnemonic token into src_data_ and the
// SECOND into dst_data_, so:
//   SrcData (= src_data_ = first token) is the PTX destination type
//   DstData (= dst_data_ = second token) is the PTX source type
// Read the source register with DstData, write the destination with SrcData.
// ---------------------------------------------------------------------------
template<dataType SrcData, dataType DstData>
void cvtInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using OutT = ptx_native_t<SrcData>; // PTX output (destination) type
    using InT  = ptx_native_t<DstData>; // PTX input  (source)      type

    InT  src_val = reg_cast<InT>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    OutT dst_val = static_cast<OutT>(src_val);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<OutT>(dst_val);
}

// ---------------------------------------------------------------------------
// bar — block barrier
// Warps within a block are executed sequentially, so bar.sync is a no-op.
// ---------------------------------------------------------------------------
void barInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)
{
    (void)wc;
}


} // namespace Ptx
} // namespace Emulator
