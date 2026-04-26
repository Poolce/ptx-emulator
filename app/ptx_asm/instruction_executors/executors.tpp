#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
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

// loc — debug line-info directive, no runtime effect
// ---------------------------------------------------------------------------
void locInstruction::ExecuteWarp(std::shared_ptr<WarpContext>& wc)
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
// shfl — Register data shuffle within threads of a warp.
//   shfl{.sync}.mode.b32  dst|pred, src, offset_reg, clamp_reg, mask{_imm};
//   dst.reg1  = value from srcLane
//   dst.reg2  = predicate (1 if srcLane was in bounds, else 0)
//   exec_type is "thread" so ExecuteThread is called per active lane, but
//   we read from other lanes' *source* registers (not yet overwritten when
//   dst != src, which is always the case in practice).
// ---------------------------------------------------------------------------
template<dataType Data>
void shflInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    uint32_t offset = static_cast<uint32_t>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);

    uint32_t src_lane = lid;
    switch (mode_) {
        case shflmodeQl::Bfly: src_lane = lid ^ offset;              break;
        case shflmodeQl::Down: src_lane = lid + offset;              break;
        case shflmodeQl::Up:   src_lane = (lid >= offset) ? lid - offset : lid; break;
        case shflmodeQl::Idx:  src_lane = offset;                    break;
        default:               src_lane = lid;
    }

    const bool valid = (src_lane < Emulator::GpuConfig::instance().warp_size)
                    && (((wc->execution_mask >> src_lane) & 1U) != 0U);
    if (!valid) {
        src_lane = lid;
    }

    wc->thread_regs[lid][dst_.reg1.type][dst_.reg1.reg_id] =
        wc->thread_regs[src_lane][src1_.type][src1_.reg_id];

    if (dst_.reg2.type != registerType::UNDEFINED) {
        wc->thread_regs[lid][dst_.reg2.type][dst_.reg2.reg_id] = valid ? 1ULL : 0ULL;
    }
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
        val = reinterpret_cast<uint64_t>(wc->getParamPtr(symbol_));
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
// div — division  (types: f16, f32, f64;  modes: rn/rz/rm/rp ignored)
// ---------------------------------------------------------------------------
template<dataType Data>
void divInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(s1 / s2);
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

    if (space_ == stspaceQl::Shared || space_ == stspaceQl::SharedCta || space_ == stspaceQl::SharedCluster)
    {
        auto* base = static_cast<uint8_t*>(wc->getSharedBase());
        addr = reinterpret_cast<uintptr_t>(base + addr);
    }

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
//
// Divergence / reconvergence model (PDOM stack):
//
//   Conditional divergence:
//     Execute the taken (if) path first; save the fall-through (else) path
//     as a pending StackFrame {fall_through_pc, fall_through_mask, false}.
//     NVCC always ends the taken path with a forward unconditional bra that
//     jumps past the fall-through block to the merge point, so the IPDOM is
//     always visible from the taken path's terminal branch.
//
//   Unconditional branch (forward) while diverged:
//     Case A — stack top is a pending path (is_convergence=false) and the
//               branch target is past the saved fall-through entry point:
//               the taken path is done; the target IS the IPDOM.
//               Pop the pending path, push a convergence frame at the target,
//               then start executing the saved fall-through path.
//     Case B — stack top is a convergence frame (is_convergence=true) whose
//               pc matches the branch target: the fall-through path arrived
//               at the IPDOM; merge masks and jump.
//     Otherwise: normal unconditional jump (loop back-edges, trampolines, …).
// ---------------------------------------------------------------------------
void braInstruction::ExecuteBranch(std::shared_ptr<WarpContext>& wc)
{
    if (prd_.type == registerType::UNDEFINED)
    {
        // ---- Unconditional branch ----
        const uint64_t target_pc = wc->GetBasicBlockPc(sym_);

        if (!wc->execution_stack.empty() && target_pc > wc->pc)
        {
            const auto& top = wc->execution_stack.top();

            if (!top.is_convergence && target_pc > top.pc)
            {
                // Case A: taken path just finished; target = IPDOM.
                // Swap to the saved fall-through path and record the
                // convergence point so it can be detected on fallthrough too.
                const uint64_t ft_pc      = top.pc;
                const uint32_t ft_mask    = top.mask;
                const uint32_t full_mask  = wc->execution_mask | ft_mask;
                wc->execution_stack.pop();
                wc->execution_stack.push({target_pc, full_mask, /*is_convergence=*/true});
                wc->pc             = ft_pc;
                wc->execution_mask = ft_mask;
                return;
            }

            if (top.is_convergence && top.pc == target_pc)
            {
                // Case B: fall-through path arrived at the IPDOM via bra.
                const uint32_t full_mask = top.mask;
                wc->execution_stack.pop();
                wc->execution_mask = full_mask;
                wc->gotoBasicBlock(sym_);
                return;
            }
        }

        wc->gotoBasicBlock(sym_);
        return;
    }

    // ---- Conditional branch ----
    const auto     pred_mask   = wc->GetPredicateMask(prd_.reg_id);
    const uint32_t branch_mask = pred_mask & wc->execution_mask;

    if (branch_mask == wc->execution_mask)
    {
        // All active threads take the branch — no divergence.
        wc->gotoBasicBlock(sym_);
    }
    else if (branch_mask == 0)
    {
        // No active thread takes the branch — fall through.
        wc->pc += 1;
    }
    else
    {
        // Diverge: execute taken (if) path first, save fall-through (else).
        const uint32_t ft_mask = wc->execution_mask & ~branch_mask;
        wc->execution_stack.push({wc->pc + 1, ft_mask, /*is_convergence=*/false});
        wc->execution_mask = branch_mask;
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

    if (space_ == ldspaceQl::Shared || space_ == ldspaceQl::SharedCta || space_ == ldspaceQl::SharedCluster)
    {
        auto* base = static_cast<uint8_t*>(wc->getSharedBase());
        addr = reinterpret_cast<uintptr_t>(base + addr);
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
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm1_));
    T s3 = (src3_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src3_.type][src3_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm2_));

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
// Convergence frames left on the stack when a path exits early (e.g. guard
// clauses) are discarded: those threads are done.
// ---------------------------------------------------------------------------
void retInstruction::ExecuteBranch(std::shared_ptr<WarpContext>& wc) // NOLINT(readability-convert-member-functions-to-static)
{
    // Skip any stale convergence frames — they belong to paths that never
    // reached the IPDOM because they exited early.
    while (!wc->execution_stack.empty() && wc->execution_stack.top().is_convergence)
    {
        wc->execution_stack.pop();
    }

    if (wc->execution_stack.empty())
    {
        wc->pc = WarpContext::EOC;
    }
    else
    {
        const auto frame   = wc->execution_stack.top();
        wc->execution_stack.pop();
        wc->pc             = frame.pc;
        wc->execution_mask = frame.mask;
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
// mode_ == Sat: clamp float→int conversions to the output type's range.
// ---------------------------------------------------------------------------
template<dataType SrcData, dataType DstData>
void cvtInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using OutT = ptx_native_t<SrcData>; // PTX output (destination) type
    using InT  = ptx_native_t<DstData>; // PTX input  (source)      type

    InT  src_val = reg_cast<InT>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    OutT dst_val;

    if (mode_ == cvtmodeQl::Sat && std::is_integral_v<OutT> && std::is_floating_point_v<InT>) {
        // Cast int limits to float for comparison, but assign the integer constant directly
        // to avoid UB from static_cast<OutT>(float_that_exceeds_int_max).
        if (src_val >= static_cast<InT>(std::numeric_limits<OutT>::max())) {
            dst_val = std::numeric_limits<OutT>::max();
        } else if (src_val <= static_cast<InT>(std::numeric_limits<OutT>::lowest())) {
            dst_val = std::numeric_limits<OutT>::lowest();
        } else {
            dst_val = static_cast<OutT>(src_val);
        }
    } else {
        dst_val = static_cast<OutT>(src_val);
    }

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<OutT>(dst_val);
}

// ---------------------------------------------------------------------------
// bar — block barrier (__syncthreads)
// In OMP mode: delegates to the cyclic BlockBarrier via syncBarrier(), which
// blocks until all active warps in the block have arrived.
// In round-robin mode: block_barrier_ is nullptr so syncBarrier() is a no-op;
// the round-robin scheduler in rt_stream.cpp handles yielding at barriers.
// ---------------------------------------------------------------------------
void barInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)
{
    wc->syncBarrier();
}

// ---------------------------------------------------------------------------
// membar — memory barrier (cta / gl / sys / cluster)
// The emulator executes instructions sequentially with no reordering, so
// membar is always a no-op.
// ---------------------------------------------------------------------------
void membarInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)
{
    (void)wc;
}

// ---------------------------------------------------------------------------
// shr — logical (b*, u*) or arithmetic (s*) shift right
// shift amount may be a register (src2_) or immediate (imm_).
// ---------------------------------------------------------------------------
template<dataType Data>
void shrInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T val = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    uint32_t sh = (src2_.type != registerType::UNDEFINED)
                    ? static_cast<uint32_t>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
                    : static_cast<uint32_t>(imm_);

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(val >> sh));
}

// ---------------------------------------------------------------------------
// or — bitwise OR  (types: pred, b16, b32, b64)
// ---------------------------------------------------------------------------
template<dataType Data>
void orInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(s1 | s2));
}

// ---------------------------------------------------------------------------
// not — bitwise NOT  (types: pred, b16, b32, b64)
// For pred, logical negation; for bit types, bitwise complement.
// ---------------------------------------------------------------------------
template<dataType Data>
void notInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T val = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    T result;
    if constexpr (std::is_same_v<T, bool>)
    {
        result = !val;
    }
    else
    {
        result = T(~val);
    }
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(result);
}

// ---------------------------------------------------------------------------
// xor — bitwise XOR  (types: pred, b16, b32, b64)
// ---------------------------------------------------------------------------
template<dataType Data>
void xorInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(s1 ^ s2));
}

// ---------------------------------------------------------------------------
// min — minimum value  (types: s*, u*, f32, f64)
// ftz_ is ignored in the emulator.
// ---------------------------------------------------------------------------
template<dataType Data>
void minInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(s1 < s2 ? s1 : s2);
}

// ---------------------------------------------------------------------------
// max — maximum value  (types: s*, u*, f32, f64)
// ftz_ is ignored in the emulator.
// ---------------------------------------------------------------------------
template<dataType Data>
void maxInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(s1 > s2 ? s1 : s2);
}


// ---------------------------------------------------------------------------
// sqrt — square root (approx or IEEE rounding, ftz ignored)
// ---------------------------------------------------------------------------
template<dataType Data>
void sqrtInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T src = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(static_cast<T>(std::sqrt(static_cast<double>(src))));
}

// ---------------------------------------------------------------------------
// sin — sine approximation (ftz ignored)
// ---------------------------------------------------------------------------
template<dataType Data>
void sinInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T src = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(static_cast<T>(std::sin(static_cast<double>(src))));
}

// ---------------------------------------------------------------------------
// cos — cosine approximation (ftz ignored)
// ---------------------------------------------------------------------------
template<dataType Data>
void cosInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T src = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(static_cast<T>(std::cos(static_cast<double>(src))));
}

// ---------------------------------------------------------------------------
// lg2 — base-2 logarithm approximation (ftz ignored)
// ---------------------------------------------------------------------------
template<dataType Data>
void lg2Instruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T src = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(static_cast<T>(std::log2(static_cast<double>(src))));
}

// ---------------------------------------------------------------------------
// rsqrt — reciprocal square root approximation (ftz ignored)
// ---------------------------------------------------------------------------
template<dataType Data>
void rsqrtInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T src = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(1) / static_cast<T>(std::sqrt(static_cast<double>(src))));
}

// ---------------------------------------------------------------------------
// popc — population count (number of set bits)
// ---------------------------------------------------------------------------
template<dataType Data>
void popcInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T val = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    uint32_t count = static_cast<uint32_t>(__builtin_popcountll(static_cast<uint64_t>(val)));
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = static_cast<uint64_t>(count);
}

// ---------------------------------------------------------------------------
// clz — count leading zeros
// ---------------------------------------------------------------------------
template<dataType Data>
void clzInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T val = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    uint32_t count;
    if constexpr (sizeof(T) == 4)
        count = val ? static_cast<uint32_t>(__builtin_clz(static_cast<uint32_t>(val))) : 32u;
    else
        count = val ? static_cast<uint32_t>(__builtin_clzll(static_cast<uint64_t>(val))) : 64u;
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = static_cast<uint64_t>(count);
}

// ---------------------------------------------------------------------------
// brev — bit reverse
// ---------------------------------------------------------------------------
template<dataType Data>
void brevInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T val = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    T result = T(0);
    constexpr int bits = sizeof(T) * 8;
    for (int i = 0; i < bits; ++i)
        result = T((result << 1) | ((val >> i) & T(1)));
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(result);
}

// ---------------------------------------------------------------------------
// bfe — bit field extract
//   dst = sign_extend((src >> pos) & mask(len))  for signed
//   dst = (src >> pos) & mask(len)               for unsigned
// pos = src2, len = src3 (or imm)
// ---------------------------------------------------------------------------
template<dataType Data>
void bfeInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    using U = std::make_unsigned_t<T>;

    T val = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    uint32_t pos = static_cast<uint32_t>(wc->thread_regs[lid][src2_.type][src2_.reg_id]) & 0xFF;
    uint32_t len = (src3_.type != registerType::UNDEFINED)
                    ? static_cast<uint32_t>(wc->thread_regs[lid][src3_.type][src3_.reg_id]) & 0xFF
                    : static_cast<uint32_t>(imm_) & 0xFF;

    if (len == 0) { wc->thread_regs[lid][dst_.type][dst_.reg_id] = 0; return; }

    constexpr uint32_t bits = sizeof(T) * 8;
    pos = std::min(pos, bits);
    len = std::min(len, bits - pos);

    U mask = len < bits ? ((U(1) << len) - 1u) : ~U(0);
    U extracted = (static_cast<U>(val) >> pos) & mask;

    T result;
    if constexpr (std::is_signed_v<T>) {
        // sign extend: if MSB of extracted field is 1, fill upper bits with 1
        if (len < bits && (extracted >> (len - 1)) & 1u)
            result = static_cast<T>(extracted | (~mask));
        else
            result = static_cast<T>(extracted);
    } else {
        result = static_cast<T>(extracted);
    }
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(result);
}

// ---------------------------------------------------------------------------
// bfi — bit field insert
//   dst = (val & ~(mask << pos)) | ((ins & mask) << pos)
// src1=ins, src2=val, src3=pos, src4=len
// ---------------------------------------------------------------------------
template<dataType Data>
void bfiInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    T ins = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T val = reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);
    uint32_t pos = static_cast<uint32_t>(wc->thread_regs[lid][src3_.type][src3_.reg_id]) & 0xFF;
    uint32_t len = (src4_.type != registerType::UNDEFINED)
                    ? static_cast<uint32_t>(wc->thread_regs[lid][src4_.type][src4_.reg_id]) & 0xFF
                    : static_cast<uint32_t>(imm_) & 0xFF;

    constexpr uint32_t bits = sizeof(T) * 8;
    if (len == 0 || pos >= bits) { wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(val); return; }
    len = std::min(len, bits - pos);

    T mask = len < bits ? T((T(1) << len) - T(1)) : T(~T(0));
    T result = T((val & ~(mask << pos)) | ((ins & mask) << pos));
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(result);
}

// ---------------------------------------------------------------------------
// bfind — find most significant bit position (returns 0xFFFFFFFF if val==0)
// sa_ (shiftamt): if true, return (msb - pos) instead of msb position
// ---------------------------------------------------------------------------
template<dataType Data>
void bfindInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T val = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);

    uint32_t result;
    if (val == T(0)) {
        result = 0xFFFFFFFFu;
    } else {
        constexpr uint32_t bits = sizeof(T) * 8;
        // for signed, find highest bit that differs from sign
        if constexpr (std::is_signed_v<T>) {
            using U = std::make_unsigned_t<T>;
            U uval = (val < 0) ? static_cast<U>(~val) : static_cast<U>(val);
            result = uval ? (bits - 1u - static_cast<uint32_t>(__builtin_clzll(static_cast<uint64_t>(uval)))) : 0u;
        } else {
            result = bits - 1u - static_cast<uint32_t>(__builtin_clzll(static_cast<uint64_t>(val)));
        }
        if (sa_) result = (sizeof(T) * 8 - 1u) - result;
    }
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = static_cast<uint64_t>(result);
}

// ---------------------------------------------------------------------------
// lop3 — 3-input bitwise logic: dst = LUT[a,b,c] (imm is the 8-bit truth table)
// ---------------------------------------------------------------------------
void lop3Instruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    uint32_t a = static_cast<uint32_t>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    uint32_t b = static_cast<uint32_t>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);
    uint32_t c = static_cast<uint32_t>(wc->thread_regs[lid][src3_.type][src3_.reg_id]);
    uint8_t lut = static_cast<uint8_t>(imm_);

    uint32_t result = 0;
    for (int bit = 31; bit >= 0; --bit) {
        uint8_t ia = (a >> bit) & 1u;
        uint8_t ib = (b >> bit) & 1u;
        uint8_t ic = (c >> bit) & 1u;
        uint8_t idx = (ia << 2) | (ib << 1) | ic;
        result |= (((lut >> idx) & 1u) << bit);
    }
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = static_cast<uint64_t>(result);
}

// ---------------------------------------------------------------------------
// prmt — permute bytes of two 32-bit values according to selector
// Default (no mode): dst[byte_i] = concat(b,a)[selector_nibble_i & 7]
//                    with sign-extension if bit 3 of nibble is set
// ---------------------------------------------------------------------------
void prmtInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    uint32_t a   = static_cast<uint32_t>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    uint32_t b   = static_cast<uint32_t>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);
    uint32_t sel = (src3_.type != registerType::UNDEFINED)
                    ? static_cast<uint32_t>(wc->thread_regs[lid][src3_.type][src3_.reg_id])
                    : static_cast<uint32_t>(imm_);

    // Bytes 0-3 from a, bytes 4-7 from b
    uint8_t bytes[8];
    for (int i = 0; i < 4; ++i) { bytes[i] = (a >> (8*i)) & 0xFF; bytes[4+i] = (b >> (8*i)) & 0xFF; }

    uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t nibble = (sel >> (4*i)) & 0xF;
        uint8_t byte_idx = nibble & 0x7;
        uint8_t byte_val = bytes[byte_idx];
        if (nibble & 0x8) byte_val = (byte_val & 0x80) ? 0xFF : 0x00; // sign-replicate
        result |= (static_cast<uint32_t>(byte_val) << (8*i));
    }
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = static_cast<uint64_t>(result);
}

// ---------------------------------------------------------------------------
// cnot — complement NOT: dst = (src == 0) ? ~0 : 0
// ---------------------------------------------------------------------------
template<dataType Data>
void cnotInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T val = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(val == T(0) ? T(~T(0)) : T(0));
}

// ---------------------------------------------------------------------------
// shf — funnel shift (left or right) with clamp/wrap
//   shf.l: dst = (b:a << c)[63:32]  (lower 32 bits of a shifted left in concat)
//   shf.r: dst = (b:a >> c)[31:0]   (upper 32 bits of a shifted right in concat)
// ---------------------------------------------------------------------------
void shfInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    uint32_t a = static_cast<uint32_t>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    uint32_t b = static_cast<uint32_t>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);
    uint32_t c = (src3_.type != registerType::UNDEFINED)
                    ? static_cast<uint32_t>(wc->thread_regs[lid][src3_.type][src3_.reg_id])
                    : static_cast<uint32_t>(imm_);

    if (mode_ == shfmodeQl::Clamp) c = std::min(c, 32u);
    else                           c &= 31u; // wrap

    uint64_t concat = (static_cast<uint64_t>(b) << 32) | a;
    uint32_t result;
    if (dir_ == shfdirQl::L)
        result = static_cast<uint32_t>((concat << c) >> 32);
    else
        result = static_cast<uint32_t>(concat >> c);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = static_cast<uint64_t>(result);
}

// ---------------------------------------------------------------------------
// mul24 — 24-bit multiply (uses only low 24 bits of each source)
// ---------------------------------------------------------------------------
template<dataType Data>
void mul24Instruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);

    // mask to 24 bits (sign-extend for signed)
    if constexpr (std::is_signed_v<T>) {
        s1 = (s1 << 8) >> 8; // sign-extend 24→32
        s2 = (s2 << 8) >> 8;
    } else {
        s1 &= T(0xFFFFFF);
        s2 &= T(0xFFFFFF);
    }

    int64_t wide = static_cast<int64_t>(s1) * static_cast<int64_t>(s2);
    T result;
    if (mode_ == mul24modeQl::Lo)
        result = static_cast<T>(wide & 0xFFFFFFFF);
    else
        result = static_cast<T>((wide >> 32) & 0xFFFFFFFF);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(result);
}

// ---------------------------------------------------------------------------
// mad24 — 24-bit multiply-add: dst = (a24 * b24) + c
// ---------------------------------------------------------------------------
template<dataType Data>
void mad24Instruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);
    T s3 = reg_cast<T>(wc->thread_regs[lid][src3_.type][src3_.reg_id]);

    if constexpr (std::is_signed_v<T>) {
        s1 = (s1 << 8) >> 8;
        s2 = (s2 << 8) >> 8;
    } else {
        s1 &= T(0xFFFFFF);
        s2 &= T(0xFFFFFF);
    }

    int64_t wide = static_cast<int64_t>(s1) * static_cast<int64_t>(s2);
    T result;
    if (mode_ == mad24modeQl::Lo)
        result = static_cast<T>((wide & 0xFFFFFFFF) + s3);
    else
        result = static_cast<T>(((wide >> 32) & 0xFFFFFFFF) + s3);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(result);
}

// ---------------------------------------------------------------------------
// rem — integer remainder
// ---------------------------------------------------------------------------
template<dataType Data>
void remInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(s2 != T(0) ? T(s1 % s2) : T(0));
}

// ---------------------------------------------------------------------------
// sad — sum of absolute differences: dst = |s1 - s2| + s3
// ---------------------------------------------------------------------------
template<dataType Data>
void sadInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);
    T s3 = reg_cast<T>(wc->thread_regs[lid][src3_.type][src3_.reg_id]);
    T diff = s1 > s2 ? T(s1 - s2) : T(s2 - s1);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(diff + s3));
}

// ---------------------------------------------------------------------------
// addc — add with carry-in (carry-in and carry-out are ignored in emulator)
// ---------------------------------------------------------------------------
template<dataType Data>
void addcInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(s1 + s2));
}

// ---------------------------------------------------------------------------
// subc — subtract with borrow (borrow ignored in emulator)
// ---------------------------------------------------------------------------
template<dataType Data>
void subcInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = (src2_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(T(s1 - s2));
}

// ---------------------------------------------------------------------------
// madc — multiply-add with carry (carry ignored)
// ---------------------------------------------------------------------------
template<dataType Data>
void madcInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T s1 = reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    T s2 = reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);
    T s3 = (src3_.type != registerType::UNDEFINED)
               ? reg_cast<T>(wc->thread_regs[lid][src3_.type][src3_.reg_id])
               : reg_cast<T>(static_cast<uint64_t>(imm_));

    uint64_t result = 0;
    switch (mode_)
    {
        case madcmodeQl::Lo:
            result = to_u64<T>(T(s1 * s2 + s3));
            break;
        case madcmodeQl::Hi:
        {
            using W = std::conditional_t<std::is_signed_v<T>, __int128, unsigned __int128>;
            W wide = W(s1) * W(s2);
            T hi   = static_cast<T>(wide >> (sizeof(T) * 8));
            result = to_u64<T>(T(hi + s3));
            break;
        }
        case madcmodeQl::Wide:
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
// slct — select based on comparison of src3
//   dst = (cmp_val >= 0.f for float, != 0 for int) ? src1 : src2
// ---------------------------------------------------------------------------
template<dataType DstType, dataType CmpType>
void slctInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using Out = ptx_native_t<DstType>;
    using Cmp = ptx_native_t<CmpType>;

    Out s1  = reg_cast<Out>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    Out s2  = reg_cast<Out>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);
    Cmp cmp = reg_cast<Cmp>(wc->thread_regs[lid][src3_.type][src3_.reg_id]);

    bool cond;
    if constexpr (std::is_floating_point_v<Cmp>)
        cond = (cmp >= Cmp(0));
    else
        cond = (cmp != Cmp(0));

    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<Out>(cond ? s1 : s2);
}

// ---------------------------------------------------------------------------
// testp — test floating-point property, result is predicate (0 or 1)
// ---------------------------------------------------------------------------
template<dataType Data>
void testpInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    T src = reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id]);

    bool result = false;
    switch (op_)
    {
        case testpopQl::Finite:     result = std::isfinite(src);   break;
        case testpopQl::Infinite:   result = std::isinf(src);      break;
        case testpopQl::Number:     result = !std::isnan(src);     break;
        case testpopQl::Notanumber: result = std::isnan(src);      break;
        case testpopQl::Normal:     result = std::isnormal(src);   break;
        case testpopQl::Subnormal:  result = std::fpclassify(src) == FP_SUBNORMAL; break;
        default: break;
    }
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = result ? 1ULL : 0ULL;
}

// ---------------------------------------------------------------------------
// szext — sign/zero extend from narrow bit-width to wider type
// ---------------------------------------------------------------------------
template<dataType Data>
void szextInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;
    uint64_t raw = wc->thread_regs[lid][src_.type][src_.reg_id];

    uint64_t result;
    switch (ext_)
    {
        case szextextQl::S8:  result = to_u64<T>(static_cast<T>(static_cast<int8_t>(raw)));   break;
        case szextextQl::S16: result = to_u64<T>(static_cast<T>(static_cast<int16_t>(raw)));  break;
        case szextextQl::U8:  result = to_u64<T>(static_cast<T>(static_cast<uint8_t>(raw)));  break;
        case szextextQl::U16: result = to_u64<T>(static_cast<T>(static_cast<uint16_t>(raw))); break;
        default:              result = raw; break;
    }
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = result;
}

// ---------------------------------------------------------------------------
// set — set on comparison: dst = (s1 cmp s2) ? 0xFFFFFFFF/1.0 : 0
// ---------------------------------------------------------------------------
template<dataType DstType, dataType SrcType>
void setInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using Src = ptx_native_t<SrcType>;
    using Dst = ptx_native_t<DstType>;

    Src s1 = reg_cast<Src>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    Src s2 = reg_cast<Src>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);

    bool cond = false;
    switch (cmp_)
    {
        case setcmpQl::Eq:  case setcmpQl::Equ: cond = (s1 == s2); break;
        case setcmpQl::Ne:  case setcmpQl::Neu: cond = (s1 != s2); break;
        case setcmpQl::Lt:  case setcmpQl::Ltu:
        case setcmpQl::Lo:  cond = (s1 < s2);  break;
        case setcmpQl::Le:  case setcmpQl::Leu:
        case setcmpQl::Ls:  cond = (s1 <= s2); break;
        case setcmpQl::Gt:  case setcmpQl::Gtu:
        case setcmpQl::Hi:  cond = (s1 > s2);  break;
        case setcmpQl::Ge:  case setcmpQl::Geu:
        case setcmpQl::Hs:  cond = (s1 >= s2); break;
        case setcmpQl::Num: cond = (s1 == s1) && (s2 == s2); break;
        case setcmpQl::Nan: cond = (s1 != s1) || (s2 != s2); break;
        default: break;
    }

    Dst out;
    if constexpr (std::is_floating_point_v<Dst>)
        out = cond ? Dst(1.0) : Dst(0.0);
    else
        out = cond ? Dst(~Dst(0)) : Dst(0);
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<Dst>(out);
}

// ---------------------------------------------------------------------------
// fns — find n-th set bit: dst = position of the n-th set bit in src1,
//   masked by src2 (or imm), starting from n-th (1-indexed) in src3 (or imm)
// Simplified: find the (src3)-th set bit in (src1 & src2).
// ---------------------------------------------------------------------------
void fnsInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    uint32_t mask  = static_cast<uint32_t>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    uint32_t val   = static_cast<uint32_t>(wc->thread_regs[lid][src2_.type][src2_.reg_id]);
    uint32_t n     = (src3_.type != registerType::UNDEFINED)
                      ? static_cast<uint32_t>(wc->thread_regs[lid][src3_.type][src3_.reg_id])
                      : static_cast<uint32_t>(imm_);

    uint32_t bits  = mask & val;
    uint32_t count = 0;
    uint32_t result = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < 32; ++i) {
        if ((bits >> i) & 1u) {
            if (++count == n) { result = i; break; }
        }
    }
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = static_cast<uint64_t>(result);
}

// ---------------------------------------------------------------------------
// bmsk — bit mask: generate a mask of 'len' ones starting at 'pos'
// ---------------------------------------------------------------------------
void bmskInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    uint32_t pos = static_cast<uint32_t>(wc->thread_regs[lid][src1_.type][src1_.reg_id]);
    uint32_t len = (src2_.type != registerType::UNDEFINED)
                    ? static_cast<uint32_t>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
                    : static_cast<uint32_t>(imm_);

    if (mode_ == bmskmodeQl::Clamp) { pos = std::min(pos, 31u); len = std::min(len, 32u - pos); }
    else                             { pos &= 31u; len &= 31u; }

    uint32_t mask = len < 32 ? ((1u << len) - 1u) << pos : ~0u;
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = static_cast<uint64_t>(mask);
}

// ---------------------------------------------------------------------------
// isspacep — test if pointer is in the given address space (always false in flat emulator)
// ---------------------------------------------------------------------------
void isspacepInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = 0ULL;
}

// ---------------------------------------------------------------------------
// istypep — test if value matches type (always false in emulator)
// ---------------------------------------------------------------------------
void istypepInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = 0ULL;
}

// ---------------------------------------------------------------------------
// createpolicy — create cache eviction policy (no-op, store 0)
// ---------------------------------------------------------------------------
void createpolicyInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = 0ULL;
}

// ---------------------------------------------------------------------------
// ldu — load from uniform address (same semantics as ld in flat emulator)
// ---------------------------------------------------------------------------
template<dataType Data>
void lduInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
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
// atom — atomic memory operation (non-atomic in emulator: single-threaded)
// ---------------------------------------------------------------------------
template<dataType Data>
void atomInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    uintptr_t addr = 0;
    if (addr_.reg.type != registerType::UNDEFINED)
        addr = static_cast<uintptr_t>(wc->thread_regs[lid][addr_.reg.type][addr_.reg.reg_id]);
    addr += static_cast<ptrdiff_t>(addr_.imm);

    T old_val = T{};
    std::memcpy(&old_val, reinterpret_cast<const void*>(addr), sizeof(T));

    T operand = (src1_.type != registerType::UNDEFINED)
                    ? reg_cast<T>(wc->thread_regs[lid][src1_.type][src1_.reg_id])
                    : reg_cast<T>(static_cast<uint64_t>(imm1_));

    T new_val = old_val;
    switch (op_)
    {
        case atomopQl::Add:  new_val = T(old_val + operand);                               break;
        case atomopQl::Min:  new_val = old_val < operand ? old_val : operand;              break;
        case atomopQl::Max:  new_val = old_val > operand ? old_val : operand;              break;
        case atomopQl::Exch: new_val = operand;                                            break;
        case atomopQl::Cas:
        {
            T cmp = (src2_.type != registerType::UNDEFINED)
                        ? reg_cast<T>(wc->thread_regs[lid][src2_.type][src2_.reg_id])
                        : reg_cast<T>(static_cast<uint64_t>(imm2_));
            new_val = (old_val == operand) ? cmp : old_val;
            break;
        }
        default:
            if constexpr (!std::is_floating_point_v<T>) {
                switch (op_) {
                    case atomopQl::And: new_val = T(old_val & operand);                                          break;
                    case atomopQl::Or:  new_val = T(old_val | operand);                                          break;
                    case atomopQl::Xor: new_val = T(old_val ^ operand);                                          break;
                    case atomopQl::Inc: new_val = (old_val >= operand) ? T(0) : T(old_val + T(1));               break;
                    case atomopQl::Dec: new_val = (old_val == T(0) || old_val > operand) ? operand : T(old_val - T(1)); break;
                    default: break;
                }
            }
            break;
    }
    std::memcpy(reinterpret_cast<void*>(addr), &new_val, sizeof(T));
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = to_u64<T>(old_val);
}

// ---------------------------------------------------------------------------
// red — reduction (like atom but no return value)
// ---------------------------------------------------------------------------
template<dataType Data>
void redInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    using T = ptx_native_t<Data>;

    uintptr_t addr = 0;
    if (addr_.reg.type != registerType::UNDEFINED)
        addr = static_cast<uintptr_t>(wc->thread_regs[lid][addr_.reg.type][addr_.reg.reg_id]);
    addr += static_cast<ptrdiff_t>(addr_.imm);

    T cur = T{};
    std::memcpy(&cur, reinterpret_cast<const void*>(addr), sizeof(T));

    T operand = (src_.type != registerType::UNDEFINED)
                    ? reg_cast<T>(wc->thread_regs[lid][src_.type][src_.reg_id])
                    : reg_cast<T>(static_cast<uint64_t>(imm_));

    T result = cur;
    switch (op_)
    {
        case redopQl::Add: result = T(cur + operand);                           break;
        case redopQl::Min: result = cur < operand ? cur : operand;              break;
        case redopQl::Max: result = cur > operand ? cur : operand;              break;
        default:
            if constexpr (!std::is_floating_point_v<T>) {
                switch (op_) {
                    case redopQl::And: result = T(cur & operand);                                      break;
                    case redopQl::Or:  result = T(cur | operand);                                      break;
                    case redopQl::Xor: result = T(cur ^ operand);                                      break;
                    case redopQl::Inc: result = (cur >= operand) ? T(0) : T(cur + T(1));               break;
                    case redopQl::Dec: result = (cur == T(0) || cur > operand) ? operand : T(cur - T(1)); break;
                    default: break;
                }
            }
            break;
    }
    std::memcpy(reinterpret_cast<void*>(addr), &result, sizeof(T));
}

// ---------------------------------------------------------------------------
// vote — warp vote (any / all / uni / ballot)
// Executes at warp level using execution_mask for active threads.
// ---------------------------------------------------------------------------
void voteInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)
{
    const uint32_t mask = wc->execution_mask;
    const uint32_t warp_size = Emulator::GpuConfig::instance().warp_size;

    // Gather predicate values across active threads
    uint32_t ballot = 0;
    uint32_t active_count = 0;
    uint32_t true_count   = 0;
    for (uint32_t lid = 0; lid < warp_size; ++lid) {
        if (!((mask >> lid) & 1u)) continue;
        ++active_count;
        uint64_t pred_val = (src_.type != registerType::UNDEFINED)
                                ? wc->thread_regs[lid][src_.type][src_.reg_id]
                                : 0ULL;
        if (pred_val) {
            ++true_count;
            ballot |= (1u << lid);
        }
    }

    for (uint32_t lid = 0; lid < warp_size; ++lid) {
        if (!((mask >> lid) & 1u)) continue;
        uint64_t result = 0;
        switch (mode_) {
            case votemodeQl::Any:    result = (true_count > 0) ? 1u : 0u; break;
            case votemodeQl::All:    result = (true_count == active_count) ? 1u : 0u; break;
            case votemodeQl::Uni:    result = (true_count == 0 || true_count == active_count) ? 1u : 0u; break;
            case votemodeQl::Ballot: result = static_cast<uint64_t>(ballot); break;
            default: break;
        }
        wc->thread_regs[lid][dst_.type][dst_.reg_id] = result;
    }
}

// ---------------------------------------------------------------------------
// activemask — returns current execution mask as b32
// ---------------------------------------------------------------------------
void activemaskInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)
{
    const uint32_t mask = wc->execution_mask;
    const uint32_t warp_size = Emulator::GpuConfig::instance().warp_size;
    for (uint32_t lid = 0; lid < warp_size; ++lid) {
        if (!((mask >> lid) & 1u)) continue;
        wc->thread_regs[lid][dst_.type][dst_.reg_id] = static_cast<uint64_t>(mask);
    }
}

// ---------------------------------------------------------------------------
// exit — terminate kernel (same as ret with empty stack)
// ---------------------------------------------------------------------------
void exitInstruction::ExecuteBranch(std::shared_ptr<WarpContext>& wc)
{
    wc->pc = WarpContext::EOC;
}

// ---------------------------------------------------------------------------
// No-op warp instructions
// ---------------------------------------------------------------------------
void trapInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)       { (void)wc; }
void fenceInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)      { (void)wc; }
void barrierInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)    { (void)wc; }
void prefetchInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)   { (void)wc; }
void prefetchuInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)  { (void)wc; }
void electInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)      { (void)wc; }
void discardInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)    { (void)wc; }
void nanosleepInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)  { (void)wc; }
void brkptInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)      { (void)wc; }
void pmeventInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)    { (void)wc; }
void mbarrierInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc)   { (void)wc; }
void applypriorityInstruction::ExecuteWarp(std::shared_ptr<Emulator::WarpContext>& wc) { (void)wc; }

// ---------------------------------------------------------------------------
// brx — indirect branch (branch to address in register — not yet supported)
// ---------------------------------------------------------------------------
void brxInstruction::ExecuteBranch(std::shared_ptr<WarpContext>& wc)
{
    wc->pc = WarpContext::EOC; // treat as exit
}

// ---------------------------------------------------------------------------
// call — function call (not supported in emulator — treat as no-op branch)
// ---------------------------------------------------------------------------
void callInstruction::ExecuteBranch(std::shared_ptr<WarpContext>& wc)
{
    wc->pc++; // skip call, treat as no-op
}

} // namespace Ptx
} // namespace Emulator
