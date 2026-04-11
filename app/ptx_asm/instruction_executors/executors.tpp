#pragma once

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

} // anonymous namespace

// ---------------------------------------------------------------------------
// reg — allocate register file slots for this thread
// ---------------------------------------------------------------------------
void regInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    wc->thread_regs[lid][reg_] = RegisterContext(count_);
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
               : static_cast<T>(imm_);

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
// add — integer addition
// ---------------------------------------------------------------------------
template<dataType Data>
void addInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : static_cast<T>(imm_);

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
    else
    {
        val = to_u64<T>(static_cast<T>(imm_));
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
// and — bitwise AND  (types: pred, b16, b32, b64)
// ---------------------------------------------------------------------------
template<dataType Data>
void andInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : static_cast<T>(imm_);

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
               : static_cast<T>(imm_);

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
                : static_cast<T>(imm_);

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
               : static_cast<T>(imm_);

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(s1 - s2));
}

// ---------------------------------------------------------------------------
// bra — conditional branch (already implemented, kept as-is)
// ---------------------------------------------------------------------------
void braInstruction::ExecuteBranch(std::shared_ptr<WarpContext>& wc)
{
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

} // namespace Ptx
} // namespace Emulator
