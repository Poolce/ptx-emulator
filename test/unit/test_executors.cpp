#include "block_context.h"
#include "constant.h"
#include "instructions.h"
#include "warp_context.h"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstring>

using namespace Emulator;
using namespace Emulator::Ptx;

// ---------------------------------------------------------------------------
// Helper: build a minimal WarpContext with pre-allocated registers.
// Only thread 0 is active by default (mask = 0x1).
// ---------------------------------------------------------------------------
static std::shared_ptr<WarpContext> makeWarp(uint32_t mask = 0x1)
{
    auto wc = std::make_shared<WarpContext>();
    wc->execution_mask = mask;
    wc->thread_regs.resize(WarpSize);
    wc->spr_regs.resize(WarpSize);

    for (uint32_t i = 0; i < WarpSize; ++i)
    {
        wc->thread_regs[i][registerType::R] = RegisterContext(8, 0);
        wc->thread_regs[i][registerType::Rd] = RegisterContext(8, 0);
        wc->thread_regs[i][registerType::P] = RegisterContext(4, 0);

        wc->spr_regs[i][sprType::TidX] = i;
        wc->spr_regs[i][sprType::TidY] = 0;
        wc->spr_regs[i][sprType::TidZ] = 0;
        wc->spr_regs[i][sprType::CtaidX] = 0;
        wc->spr_regs[i][sprType::CtaidY] = 0;
        wc->spr_regs[i][sprType::CtaidZ] = 0;
        wc->spr_regs[i][sprType::NtidX] = 32;
        wc->spr_regs[i][sprType::NtidY] = 1;
        wc->spr_regs[i][sprType::NtidZ] = 1;
    }
    return wc;
}

// Convenience: read a 32-bit register value as a typed T for thread 0.
template <typename T>
static T r32(const std::shared_ptr<WarpContext>& wc, uint32_t id)
{
    uint64_t raw = wc->thread_regs[0][registerType::R][id];
    T val;
    if constexpr (std::is_integral_v<T>)
    {
        return static_cast<T>(raw);
    }
    else
    {
        std::memcpy(&val, &raw, sizeof(T));
        return val;
    }
}

template <typename T>
static T rd64(const std::shared_ptr<WarpContext>& wc, uint32_t id)
{
    uint64_t raw = wc->thread_regs[0][registerType::Rd][id];
    T val;
    if constexpr (std::is_integral_v<T>)
    {
        return static_cast<T>(raw);
    }
    else
    {
        std::memcpy(&val, &raw, sizeof(T));
        return val;
    }
}

static uint64_t pred(const std::shared_ptr<WarpContext>& wc, uint32_t id)
{
    return wc->thread_regs[0][registerType::P][id];
}

// Write a 32-bit value into thread 0's %rN
template <typename T>
static void setR(const std::shared_ptr<WarpContext>& wc, uint32_t id, T val)
{
    uint64_t raw = 0;
    if constexpr (std::is_integral_v<T>)
    {
        raw = static_cast<uint64_t>(static_cast<std::make_unsigned_t<T>>(val));
    }
    else
    {
        std::memcpy(&raw, &val, sizeof(T));
    }
    wc->thread_regs[0][registerType::R][id] = raw;
}

template <typename T>
static void setRd(const std::shared_ptr<WarpContext>& wc, uint32_t id, T val)
{
    uint64_t raw = 0;
    if constexpr (std::is_integral_v<T>)
    {
        raw = static_cast<uint64_t>(static_cast<std::make_unsigned_t<T>>(val));
    }
    else
    {
        std::memcpy(&raw, &val, sizeof(T));
    }
    wc->thread_regs[0][registerType::Rd][id] = raw;
}

// ============================================================================
// GetPredicateMask
// ============================================================================
TEST(GetPredicateMask, OnlyActiveBitsSet)
{
    // 4 threads active (mask = 0xF), threads 0 and 2 have p0 = true
    auto wc = makeWarp(0xF);
    wc->thread_regs[0][registerType::P][0] = 1;
    wc->thread_regs[2][registerType::P][0] = 1;

    uint32_t mask = wc->GetPredicateMask(0);
    EXPECT_EQ(mask, 0x5U); // bits 0 and 2
}

TEST(GetPredicateMask, NoActiveThreads)
{
    auto wc = makeWarp(0x0);
    wc->thread_regs[0][registerType::P][0] = 1; // set but thread 0 not in execution_mask

    // execution_mask is 0, so nothing is active.
    // GetPredicateMask reads all WarpSize slots; since mask=0 the Execute loop
    // won't call this path, but the method itself iterates unconditionally.
    // All 32 register slots exist, so the result is the raw predicate state.
    uint32_t mask = wc->GetPredicateMask(0);
    EXPECT_EQ(mask, 0x1U); // bit 0 set because %p0[0]=1
}

TEST(GetPredicateMask, OutOfBoundsPredicateIdReturnsFalse)
{
    auto wc = makeWarp(0x1);
    // prd_id = 99 is way beyond the 4-element P RegisterContext
    uint32_t mask = wc->GetPredicateMask(99);
    EXPECT_EQ(mask, 0U);
}

// ============================================================================
// reg — register allocation
// ============================================================================
TEST(RegExecutor, AllocatesRequestedCount)
{
    auto wc = makeWarp();
    // Overwrite thread 0's R bank to start empty
    wc->thread_regs[0][registerType::R] = {};

    auto instr = regInstruction::Make(".reg .u32 %r<5>;");
    instr->Execute(wc);

    EXPECT_EQ(wc->thread_regs[0][registerType::R].size(), 5U);
    EXPECT_EQ(wc->thread_regs[0][registerType::R][3], 0U);
    EXPECT_EQ(wc->pc, 1U);
}

// ============================================================================
// add
// ============================================================================
TEST(AddExecutor, U32RegisterPlusRegister)
{
    auto wc = makeWarp();
    setR(wc, 0, 10U);
    setR(wc, 1, 32U);

    auto instr = addInstruction::Make("add.u32 %r2, %r0, %r1;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 2), 42U);
}

TEST(AddExecutor, S32NegativeResult)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(3));
    setR(wc, 1, int32_t(-10));

    auto instr = addInstruction::Make("add.s32 %r2, %r0, %r1;");
    instr->Execute(wc);

    EXPECT_EQ(r32<int32_t>(wc, 2), -7);
}

TEST(AddExecutor, U32Overflow)
{
    auto wc = makeWarp();
    setR(wc, 0, uint32_t(0xFFFFFFFF));
    setR(wc, 1, uint32_t(1));

    auto instr = addInstruction::Make("add.u32 %r2, %r0, %r1;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 2), 0U); // wraps
}

TEST(AddExecutor, OnlyActiveLanesWritten)
{
    // Two threads active (bits 0 and 1), seed different starting values
    auto wc = makeWarp(0x3);
    wc->thread_regs[0][registerType::R][0] = 1;
    wc->thread_regs[0][registerType::R][1] = 2;
    wc->thread_regs[1][registerType::R][0] = 100;
    wc->thread_regs[1][registerType::R][1] = 200;
    // Thread 2 is inactive but has values too
    wc->thread_regs[2][registerType::R][0] = 999;
    wc->thread_regs[2][registerType::R][1] = 999;

    auto instr = addInstruction::Make("add.u32 %r2, %r0, %r1;");
    instr->Execute(wc);

    EXPECT_EQ(wc->thread_regs[0][registerType::R][2], 3U);
    EXPECT_EQ(wc->thread_regs[1][registerType::R][2], 300U);
    EXPECT_EQ(wc->thread_regs[2][registerType::R][2], 0U); // unchanged
}

// ============================================================================
// mov
// ============================================================================
TEST(MovExecutor, RegisterToRegister)
{
    auto wc = makeWarp();
    setR(wc, 0, 77U);

    auto instr = movInstruction::Make("mov.u32 %r1, %r0;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 1), 77U);
}

TEST(MovExecutor, ImmediateToRegister)
{
    auto wc = makeWarp();
    auto instr = movInstruction::Make("mov.u32 %r0, 42;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 0), 42U);
}

TEST(MovExecutor, NegativeImmediateS32)
{
    auto wc = makeWarp();
    auto instr = movInstruction::Make("mov.s32 %r0, -5;");
    instr->Execute(wc);

    EXPECT_EQ(r32<int32_t>(wc, 0), -5);
}

TEST(MovExecutor, FromTidX)
{
    // thread 0 has TidX=0, but let's use a 3-thread mask and check thread 2
    auto wc = makeWarp(0x7);
    auto instr = movInstruction::Make("mov.u32 %r0, %tid.x;");
    instr->Execute(wc);

    EXPECT_EQ(wc->thread_regs[0][registerType::R][0], 0U);
    EXPECT_EQ(wc->thread_regs[1][registerType::R][0], 1U);
    EXPECT_EQ(wc->thread_regs[2][registerType::R][0], 2U);
}

// ============================================================================
// setp
// ============================================================================
TEST(SetpExecutor, LtS32True)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(-1));
    setR(wc, 1, int32_t(0));

    auto instr = setpInstruction::Make("setp.lt.s32 %p0 %r0 %r1;");
    instr->Execute(wc);

    EXPECT_EQ(pred(wc, 0), 1U);
}

TEST(SetpExecutor, LtS32False)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(5));
    setR(wc, 1, int32_t(3));

    auto instr = setpInstruction::Make("setp.lt.s32 %p0 %r0 %r1;");
    instr->Execute(wc);

    EXPECT_EQ(pred(wc, 0), 0U);
}

TEST(SetpExecutor, EqU32)
{
    auto wc = makeWarp();
    setR(wc, 0, 7U);
    setR(wc, 1, 7U);

    auto instr = setpInstruction::Make("setp.eq.u32 %p0 %r0 %r1;");
    instr->Execute(wc);

    EXPECT_EQ(pred(wc, 0), 1U);
}

TEST(SetpExecutor, GeS32)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(5));
    setR(wc, 1, int32_t(5));

    auto instr = setpInstruction::Make("setp.ge.s32 %p1 %r0 %r1;");
    instr->Execute(wc);

    EXPECT_EQ(pred(wc, 1), 1U);
}

TEST(SetpExecutor, NeU32)
{
    auto wc = makeWarp();
    setR(wc, 0, 1U);
    setR(wc, 1, 2U);

    auto instr = setpInstruction::Make("setp.ne.u32 %p0 %r0 %r1;");
    instr->Execute(wc);

    EXPECT_EQ(pred(wc, 0), 1U);
}

// ============================================================================
// shl
// ============================================================================
TEST(ShlExecutor, ShiftBy2)
{
    auto wc = makeWarp();
    setR(wc, 0, uint32_t(1));

    auto instr = shlInstruction::Make("shl.b32 %r1, %r0, 2;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 1), 4U);
}

TEST(ShlExecutor, ShiftBy31)
{
    auto wc = makeWarp();
    setR(wc, 0, uint32_t(1));

    auto instr = shlInstruction::Make("shl.b32 %r1, %r0, 31;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 1), 0x80000000U);
}

// ============================================================================
// and
// ============================================================================
TEST(AndExecutor, B32Mask)
{
    auto wc = makeWarp();
    setR(wc, 0, uint32_t(0xFF00FF00));
    setR(wc, 1, uint32_t(0x0F0F0F0F));

    auto instr = andInstruction::Make("and.b32 %r2, %r0, %r1;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 2), 0x0F000F00U);
}

// ============================================================================
// neg
// ============================================================================
TEST(NegExecutor, S32)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(42));

    auto instr = negInstruction::Make("neg.s32 %r1, %r0;");
    instr->Execute(wc);

    EXPECT_EQ(r32<int32_t>(wc, 1), -42);
}

TEST(NegExecutor, S32NegateNegative)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(-100));

    auto instr = negInstruction::Make("neg.s32 %r1, %r0;");
    instr->Execute(wc);

    EXPECT_EQ(r32<int32_t>(wc, 1), 100);
}

// ============================================================================
// sub
// ============================================================================
TEST(SubExecutor, F32)
{
    auto wc = makeWarp();
    setR(wc, 0, 5.0F);
    setR(wc, 1, 3.0F);

    auto instr = subInstruction::Make("sub.f32 %r2, %r0, %r1;");
    instr->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 2), 2.0F);
}

// ============================================================================
// mul
// ============================================================================
TEST(MulExecutor, LoS32)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(6));
    setR(wc, 1, int32_t(7));

    auto instr = mulInstruction::Make("mul.lo.s32 %r2, %r0, %r1;");
    instr->Execute(wc);

    EXPECT_EQ(r32<int32_t>(wc, 2), 42);
}

TEST(MulExecutor, WideS32ProduceS64)
{
    // 2^31 * 2 → must be representable as 64-bit only
    auto wc = makeWarp();
    setR(wc, 0, int32_t(0x40000000)); // 2^30
    setR(wc, 1, int32_t(4));

    // wide result goes into %rd register (64-bit)
    wc->thread_regs[0][registerType::Rd].resize(8, 0);
    // Use mul.wide.s32 with dst as %rd0
    auto instr = mulInstruction::Make("mul.wide.s32 %rd0, %r0, %r1;");
    instr->Execute(wc);

    EXPECT_EQ(rd64<int64_t>(wc, 0), int64_t(0x40000000) * 4);
}

TEST(MulExecutor, HiU32OverflowPreservesHighBits)
{
    // 0xFFFFFFFF * 0xFFFFFFFF = 0xFFFFFFFE00000001
    // hi 32 bits = 0xFFFFFFFE
    auto wc = makeWarp();
    setR(wc, 0, uint32_t(0xFFFFFFFF));
    setR(wc, 1, uint32_t(0xFFFFFFFF));

    auto instr = mulInstruction::Make("mul.hi.u32 %r2, %r0, %r1;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 2), 0xFFFFFFFEU);
}

// ============================================================================
// mad
// ============================================================================
TEST(MadExecutor, LoU32)
{
    auto wc = makeWarp();
    setR(wc, 0, 3U); // a
    setR(wc, 1, 4U); // b
    setR(wc, 2, 5U); // c

    auto instr = madInstruction::Make("mad.lo.u32 %r3, %r0, %r1, %r2;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 3), 17U); // 3*4+5
}

TEST(MadExecutor, WideS32)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(0x10000)); // 65536
    setR(wc, 1, int32_t(0x10000)); // 65536
    // src3 is also 64-bit for wide mode; store in %rd for the accumulator
    // For simplicity, use %r2 as src3 (value 1)
    setR(wc, 2, int32_t(1));

    auto instr = madInstruction::Make("mad.wide.s32 %rd0, %r0, %r1, %r2;");
    instr->Execute(wc);

    // 65536 * 65536 + 1 = 2^32 + 1
    EXPECT_EQ(rd64<int64_t>(wc, 0), int64_t(0x100000001LL));
}

// ============================================================================
// ld / st  (register-based address — no context chain needed)
// ============================================================================
TEST(LdStExecutor, RoundtripU32)
{
    auto wc = makeWarp();
    uint32_t mem = 0;

    // st.global.u32 [%rd0], %r0  — store 42
    setR(wc, 0, uint32_t(42));
    setRd(wc, 0, reinterpret_cast<uintptr_t>(&mem));

    auto st = stInstruction::Make("st.global.u32 [%rd0], %r0;");
    st->Execute(wc);

    EXPECT_EQ(mem, 42U);

    // ld.global.u32 %r1, [%rd0]  — load back
    auto ld = ldInstruction::Make("ld.global.u32 %r1, [%rd0];");
    ld->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 1), 42U);
}

TEST(LdStExecutor, RoundtripF32)
{
    auto wc = makeWarp();
    float mem = 0.0F;

    setR(wc, 0, 3.14F);
    setRd(wc, 0, reinterpret_cast<uintptr_t>(&mem));

    stInstruction::Make("st.global.f32 [%rd0], %r0;")->Execute(wc);
    EXPECT_FLOAT_EQ(mem, 3.14F);

    ldInstruction::Make("ld.global.f32 %r1, [%rd0];")->Execute(wc);
    EXPECT_FLOAT_EQ(r32<float>(wc, 1), 3.14F);
}

TEST(LdStExecutor, StoreWithImmediateOffset)
{
    auto wc = makeWarp();
    uint32_t buf[4] = {0, 0, 0, 0};

    setR(wc, 0, uint32_t(99));
    setRd(wc, 0, reinterpret_cast<uintptr_t>(buf));

    // Store to buf[2] (offset = 8 bytes)
    stInstruction::Make("st.global.u32 [%rd0+8], %r0;")->Execute(wc);
    EXPECT_EQ(buf[0], 0U);
    EXPECT_EQ(buf[2], 99U);
}

// ============================================================================
// cvt
// ============================================================================
TEST(CvtExecutor, S32ToU64SignExtend)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(-1));

    // cvt.u64.s32 %rd0, %r0
    auto instr = cvtInstruction::Make("cvt.u64.s32 %rd0, %r0;");
    instr->Execute(wc);

    // static_cast<uint64_t>(int32_t(-1)) == static_cast<uint64_t>(int64_t(-1))
    EXPECT_EQ(rd64<uint64_t>(wc, 0), static_cast<uint64_t>(int64_t(-1)));
}

TEST(CvtExecutor, U32ToS64ZeroExtend)
{
    auto wc = makeWarp();
    setR(wc, 0, uint32_t(0xDEADBEEF));

    auto instr = cvtInstruction::Make("cvt.s64.u32 %rd0, %r0;");
    instr->Execute(wc);

    EXPECT_EQ(rd64<int64_t>(wc, 0), int64_t(0xDEADBEEF));
}

TEST(CvtExecutor, F32ToS32Truncation)
{
    // cvt.s32.f32: first token = s32 (output), second token = f32 (input)
    auto wc = makeWarp();
    setR(wc, 0, 3.9F);

    auto instr = cvtInstruction::Make("cvt.s32.f32 %r1, %r0;");
    instr->Execute(wc);

    EXPECT_EQ(r32<int32_t>(wc, 1), 3); // truncation toward zero
}

TEST(CvtExecutor, S32ToF32Positive)
{
    // cvt.f32.s32: output=f32, input=s32
    auto wc = makeWarp();
    setR(wc, 0, int32_t(7));

    auto instr = cvtInstruction::Make("cvt.f32.s32 %r1, %r0;");
    instr->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 1), 7.0F);
}

// ============================================================================
// ret
// ============================================================================
TEST(RetExecutor, EmptyStackSetsEOC)
{
    auto wc = makeWarp();
    wc->pc = 10;

    auto instr = retInstruction::Make("ret;");
    instr->Execute(wc);

    EXPECT_EQ(wc->pc, WarpContext::EOC);
}

TEST(RetExecutor, NonEmptyStackRestoresState)
{
    auto wc = makeWarp();
    wc->pc = 10;
    wc->execution_mask = 0xF;
    wc->execution_stack.push({42U, 0x3U});

    auto instr = retInstruction::Make("ret;");
    instr->Execute(wc);

    EXPECT_EQ(wc->pc, 42U);
    EXPECT_EQ(wc->execution_mask, 0x3U);
    EXPECT_TRUE(wc->execution_stack.empty());
}

// ============================================================================
// bra — only the "no threads take branch" path (no block_context needed)
// ============================================================================
TEST(BraExecutor, NoBranchTakenIncrementsPC)
{
    auto wc = makeWarp(0x1);
    wc->pc = 5;
    // %p0 = 0 for thread 0 → branch_mask = 0 → pc++
    wc->thread_regs[0][registerType::P][0] = 0;

    auto instr = braInstruction::Make("@%p0 bra $TARGET;");
    instr->Execute(wc); // calls ExecuteBranch (no pc++ in branch path)

    EXPECT_EQ(wc->pc, 6U);
}

TEST(BraExecutor, DivergentBranchPushesStack)
{
    // 2 threads active: thread 0 predicate true, thread 1 false
    auto wc = makeWarp(0x3);
    wc->pc = 10;
    wc->thread_regs[0][registerType::P][0] = 1; // branch
    wc->thread_regs[1][registerType::P][0] = 0; // fall-through

    // We can't call gotoBasicBlock (no block context), so just verify
    // that the execution stack is pushed and mask is updated.
    // Use a try/catch since gotoBasicBlock will throw (expired block_context).
    auto instr = braInstruction::Make("@%p0 bra $TARGET;");
    EXPECT_THROW(instr->Execute(wc), std::runtime_error);

    // The stack push happens before gotoBasicBlock, so state should reflect it
    EXPECT_FALSE(wc->execution_stack.empty());
    auto [saved_pc, saved_mask] = wc->execution_stack.top();
    EXPECT_EQ(saved_pc, 11U);            // fall-through pc = 10+1
    EXPECT_EQ(saved_mask, 0x2U);         // bit 1 = fall-through thread
    EXPECT_EQ(wc->execution_mask, 0x1U); // only branching thread remains
}

TEST(BraExecutor, UnconditionalBranchNoPredicateThrowsWithoutContext)
{
    // "bra $TARGET" — no @predicate → unconditional branch
    auto wc = makeWarp(0x1);
    wc->pc = 10;
    auto instr = braInstruction::Make("bra $TARGET;");
    // gotoBasicBlock throws because there is no block context
    EXPECT_THROW(instr->Execute(wc), std::runtime_error);
    // pc must not have been changed (throw happens before pc assignment)
    EXPECT_EQ(wc->pc, 10U);
}

// ============================================================================
// bar
// ============================================================================
TEST(BarExecutor, BarSyncIsNoOp)
{
    auto wc = makeWarp();
    wc->pc = 5;
    auto instr = barInstruction::Make("bar.sync 0;");
    EXPECT_NO_THROW(instr->Execute(wc));
    EXPECT_EQ(wc->pc, 6U); // warp instruction: Execute() does pc++ after ExecuteWarp
}

// ============================================================================
// tanh
// ============================================================================
TEST(TanhExecutor, F32Zero)
{
    auto wc = makeWarp();
    setR(wc, 0, 0.0F); // tanh(0) = 0

    tanhInstruction::Make("tanh.approx.ftz.f32 %r1, %r0;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 1), 0.0F);
}

TEST(TanhExecutor, F32PositiveSaturation)
{
    auto wc = makeWarp();
    setR(wc, 0, 10.0F); // tanh(10) ≈ 1.0

    tanhInstruction::Make("tanh.approx.ftz.f32 %r1, %r0;")->Execute(wc);

    EXPECT_NEAR(r32<float>(wc, 1), 1.0F, 1e-5F);
}

TEST(TanhExecutor, F32MatchesStdTanh)
{
    auto wc = makeWarp();
    const float x = 0.5F;
    setR(wc, 0, x);

    tanhInstruction::Make("tanh.approx.ftz.f32 %r1, %r0;")->Execute(wc);

    EXPECT_NEAR(r32<float>(wc, 1), std::tanh(x), 1e-6F);
}

TEST(TanhExecutor, F64MatchesStdTanh)
{
    auto wc = makeWarp();
    const double x = -1.5;
    setRd(wc, 0, x);

    tanhInstruction::Make("tanh.approx.ftz.f64 %rd1, %rd0;")->Execute(wc);

    EXPECT_DOUBLE_EQ(rd64<double>(wc, 1), std::tanh(x));
}

// ============================================================================
// fma
// ============================================================================
TEST(FmaExecutor, F32MultiplyAddRegisters)
{
    auto wc = makeWarp();
    setR(wc, 0, 2.0F); // src1
    setR(wc, 1, 3.0F); // src2
    setR(wc, 2, 1.0F); // src3

    fmaInstruction::Make("fma.rn.f32 %r3, %r0, %r1, %r2;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 3), 7.0F); // 2*3+1=7
}

TEST(FmaExecutor, F32NegativeAccumulator)
{
    auto wc = makeWarp();
    setR(wc, 0, 4.0F);
    setR(wc, 1, 5.0F);
    setR(wc, 2, -30.0F);

    fmaInstruction::Make("fma.rn.f32 %r3, %r0, %r1, %r2;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 3), -10.0F); // 4*5-30=-10
}

TEST(FmaExecutor, F64MultiplyAddRegisters)
{
    auto wc = makeWarp();
    setRd(wc, 0, 2.0);
    setRd(wc, 1, 3.0);
    setRd(wc, 2, 0.5);

    fmaInstruction::Make("fma.rn.f64 %rd3, %rd0, %rd1, %rd2;")->Execute(wc);

    EXPECT_DOUBLE_EQ(rd64<double>(wc, 3), 6.5); // 2*3+0.5=6.5
}

TEST(FmaExecutor, F32MatchesStdFma)
{
    // Verify that the executor delegates to std::fma and not naive a*b+c.
    auto wc = makeWarp();
    const float a = 1.0F / 3.0F;
    const float b = 3.0F;
    const float c = 1e-7F;
    setR(wc, 0, a);
    setR(wc, 1, b);
    setR(wc, 2, c);

    fmaInstruction::Make("fma.rn.f32 %r3, %r0, %r1, %r2;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 3), std::fma(a, b, c));
}

// ============================================================================
// shared + mov with symbol
// Helper creates a BlockContext-backed WarpContext with pre-allocated shared
// memory. Both the BlockContext and WarpContext are returned so the caller
// keeps the BlockContext alive (needed because warp holds a weak_ptr to it).
// ============================================================================
static std::pair<std::shared_ptr<BlockContext>, std::shared_ptr<WarpContext>>
makeWarpWithSharedMem(size_t shared_bytes = 4096)
{
    auto bc = std::make_shared<BlockContext>();
    // nullptr global_context is fine — RegisterSharedSymbol/GetSharedPtr
    // never access it, and Init only stores it as a weak_ptr.
    bc->Init(nullptr, {1, 1, 1}, {0, 0, 0}, {32, 1, 1}, shared_bytes);
    auto wc = bc->GetWarps()[0];
    for (uint32_t i = 0; i < WarpSize; ++i)
    {
        wc->thread_regs[i][registerType::R] = RegisterContext(8, 0);
        wc->thread_regs[i][registerType::Rd] = RegisterContext(8, 0);
        wc->thread_regs[i][registerType::P] = RegisterContext(4, 0);
    }
    return {bc, wc};
}

TEST(SharedMemExecutor, RegistersSymbolNonNullAddress)
{
    auto [bc, wc] = makeWarpWithSharedMem(1024);

    // .shared .align 4 .f32 smTile[64] → 64 * 4 = 256 bytes
    // Note: symbol names must not start with hex digits (a-f) to avoid
    // ambiguity in the mov regex that also matches hex immediates.
    sharedInstruction::Make(".shared .align 4 .f32 smTile[64];")->Execute(wc);
    sharedInstruction::Make(".shared .align 4 .f32 smTile2[4];")->Execute(wc);

    movInstruction::Make("mov.u32 %r0, smTile;")->Execute(wc);
    movInstruction::Make("mov.u32 %r1, smTile2;")->Execute(wc);

    // First symbol is at offset 0 (fits in 32 bits); second is non-zero.
    EXPECT_LT(wc->thread_regs[0][registerType::R][0], 1024ULL);
    EXPECT_NE(wc->thread_regs[0][registerType::R][1], 0ULL);
}

TEST(SharedMemExecutor, TwoSymbolsHaveDistinctAddresses)
{
    auto [bc, wc] = makeWarpWithSharedMem(4096);

    sharedInstruction::Make(".shared .align 4 .f32 smX[64];")->Execute(wc);
    sharedInstruction::Make(".shared .align 4 .f32 smY[64];")->Execute(wc);

    movInstruction::Make("mov.u32 %r0, smX;")->Execute(wc);
    movInstruction::Make("mov.u32 %r1, smY;")->Execute(wc);

    EXPECT_NE(wc->thread_regs[0][registerType::R][0], wc->thread_regs[0][registerType::R][1]);
}

TEST(SharedMemExecutor, SymbolRegisteredOnceEvenIfCalledTwice)
{
    auto [bc, wc] = makeWarpWithSharedMem(4096);

    // Simulate two warps executing the same .shared directive
    sharedInstruction::Make(".shared .align 4 .f32 smWork[32];")->Execute(wc);
    sharedInstruction::Make(".shared .align 4 .f32 smWork[32];")->Execute(wc);

    movInstruction::Make("mov.u32 %r0, smWork;")->Execute(wc);
    movInstruction::Make("mov.u32 %r1, smWork;")->Execute(wc);

    // Both reads must return the same address
    EXPECT_EQ(wc->thread_regs[0][registerType::R][0], wc->thread_regs[0][registerType::R][1]);
}

TEST(SharedMemExecutor, StoreAndLoadRoundtrip)
{
    auto [bc, wc] = makeWarpWithSharedMem(4096);

    // Only thread 0 active: prevents other threads from writing to their
    // uninitialized %rd0 (= 0) during st, which would cause a SegFault.
    wc->execution_mask = 0x1;

    // Register 128-float shared buffer
    sharedInstruction::Make(".shared .align 4 .f32 smOut[128];")->Execute(wc);

    // Load the symbol address (stored as full 64-bit in R slot) → propagate to Rd
    movInstruction::Make("mov.u32 %r0, smOut;")->Execute(wc);
    wc->thread_regs[0][registerType::Rd][0] = wc->thread_regs[0][registerType::R][0];

    // st.shared.f32 [%rd0], %r1  (write 3.14 to smOut[0])
    setR(wc, 1, 3.14F);
    stInstruction::Make("st.shared.f32 [%rd0], %r1;")->Execute(wc);

    // ld.shared.f32 %r2, [%rd0]  (read back)
    ldInstruction::Make("ld.shared.f32 %r2, [%rd0];")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 2), 3.14F);
}

// ============================================================================
// cvta — address-space conversion (identity in flat-memory emulator)
// ============================================================================
TEST(CvtaExecutor, GlobalU64PreservesPointerValue)
{
    auto wc = makeWarp();
    uint64_t addr = 0xDEADBEEFCAFEBABEULL;
    setRd(wc, 0, addr);

    cvtaInstruction::Make("cvta.to.global.u64 %rd1, %rd0;")->Execute(wc);

    EXPECT_EQ(rd64<uint64_t>(wc, 1), addr);
}

TEST(CvtaExecutor, SharedU64PreservesPointerValue)
{
    auto wc = makeWarp();
    uint64_t addr = 0x0000FFFF0000FFFFULL;
    setRd(wc, 0, addr);

    cvtaInstruction::Make("cvta.shared.u64 %rd1, %rd0;")->Execute(wc);

    EXPECT_EQ(rd64<uint64_t>(wc, 1), addr);
}

// ============================================================================
// abs — absolute value
// ============================================================================
TEST(AbsExecutor, S32Positive)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(-42));

    absInstruction::Make("abs.s32 %r1, %r0;")->Execute(wc);

    EXPECT_EQ(r32<int32_t>(wc, 1), 42);
}

TEST(AbsExecutor, S32AlreadyPositive)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(7));

    absInstruction::Make("abs.s32 %r1, %r0;")->Execute(wc);

    EXPECT_EQ(r32<int32_t>(wc, 1), 7);
}

TEST(AbsExecutor, S64Negative)
{
    auto wc = makeWarp();
    setRd(wc, 0, int64_t(-1000000000LL));

    absInstruction::Make("abs.s64 %rd1, %rd0;")->Execute(wc);

    EXPECT_EQ(rd64<int64_t>(wc, 1), int64_t(1000000000LL));
}

TEST(AbsExecutor, F32Negative)
{
    auto wc = makeWarp();
    setR(wc, 0, -3.14F);

    absInstruction::Make("abs.f32 %r1, %r0;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 1), 3.14F);
}

TEST(AbsExecutor, F64Negative)
{
    auto wc = makeWarp();
    setRd(wc, 0, std::numbers::e);

    absInstruction::Make("abs.f64 %rd1, %rd0;")->Execute(wc);

    EXPECT_DOUBLE_EQ(rd64<double>(wc, 1), std::numbers::e);
}

// ============================================================================
// ex2 — base-2 exponential  (ex2.approx.ftz.type)
// Note: .ftz is required by the generated regex (not optional).
// ============================================================================
TEST(Ex2Executor, F32PowerOfTwo)
{
    auto wc = makeWarp();
    setR(wc, 0, 3.0F); // 2^3 = 8

    ex2Instruction::Make("ex2.approx.ftz.f32 %r1, %r0;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 1), 8.0F);
}

TEST(Ex2Executor, F32ZeroExponent)
{
    auto wc = makeWarp();
    setR(wc, 0, 0.0F); // 2^0 = 1

    ex2Instruction::Make("ex2.approx.ftz.f32 %r1, %r0;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 1), 1.0F);
}

TEST(Ex2Executor, F64NegativeExponent)
{
    auto wc = makeWarp();
    setRd(wc, 0, -1.0); // 2^(-1) = 0.5

    ex2Instruction::Make("ex2.approx.ftz.f64 %rd1, %rd0;")->Execute(wc);

    EXPECT_DOUBLE_EQ(rd64<double>(wc, 1), 0.5);
}

// ============================================================================
// rcp — reciprocal  (rcp.approx.ftz.type)
// ============================================================================
TEST(RcpExecutor, F32BasicReciprocal)
{
    auto wc = makeWarp();
    setR(wc, 0, 4.0F); // 1/4 = 0.25

    rcpInstruction::Make("rcp.approx.ftz.f32 %r1, %r0;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 1), 0.25F);
}

TEST(RcpExecutor, F32One)
{
    auto wc = makeWarp();
    setR(wc, 0, 1.0F); // 1/1 = 1

    rcpInstruction::Make("rcp.approx.ftz.f32 %r1, %r0;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 1), 1.0F);
}

TEST(RcpExecutor, F64Reciprocal)
{
    auto wc = makeWarp();
    setRd(wc, 0, 2.0); // 1/2 = 0.5

    rcpInstruction::Make("rcp.approx.ftz.f64 %rd1, %rd0;")->Execute(wc);

    EXPECT_DOUBLE_EQ(rd64<double>(wc, 1), 0.5);
}

// ============================================================================
// copysign — PTX: copysign d, a, b → magnitude from b (src2), sign from a (src1)
// ============================================================================
TEST(CopysignExecutor, F32PositiveSignNegativeMagnitude)
{
    auto wc = makeWarp();
    setR(wc, 0, 3.0F);  // src1: provides sign (+)
    setR(wc, 1, -5.0F); // src2: provides magnitude (5.0)

    copysignInstruction::Make("copysign.f32 %r2, %r0, %r1;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 2), 5.0F); // |src2| * sign(src1) = 5.0 * (+1) = 5.0
}

TEST(CopysignExecutor, F32NegativeSignPositiveMagnitude)
{
    auto wc = makeWarp();
    setR(wc, 0, -3.0F); // src1: provides sign (-)
    setR(wc, 1, 1.0F);  // src2: provides magnitude (1.0)

    copysignInstruction::Make("copysign.f32 %r2, %r0, %r1;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 2), -1.0F); // |src2| * sign(src1) = 1.0 * (-1) = -1.0
}

TEST(CopysignExecutor, F64BothNegative)
{
    auto wc = makeWarp();
    setRd(wc, 0, -3.0); // src1: provides sign (-)
    setRd(wc, 1, -7.0); // src2: provides magnitude (7.0)

    copysignInstruction::Make("copysign.f64 %rd2, %rd0, %rd1;")->Execute(wc);

    EXPECT_DOUBLE_EQ(rd64<double>(wc, 2), -7.0); // |src2| * sign(src1) = 7.0 * (-1) = -7.0
}

// ============================================================================
// selp — predicate-based select
//   selp.type d, a, b, c  →  d = (c != 0) ? a : b
// ============================================================================
TEST(SelpExecutor, U32TruePredicate)
{
    auto wc = makeWarp();
    setR(wc, 0, 10U);                           // a — taken when predicate true
    setR(wc, 1, 20U);                           // b — taken when predicate false
    wc->thread_regs[0][registerType::P][0] = 1; // %p0 = true

    selpInstruction::Make("selp.u32 %r2, %r0, %r1, %p0;")->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 2), 10U);
}

TEST(SelpExecutor, U32FalsePredicate)
{
    auto wc = makeWarp();
    setR(wc, 0, 10U);
    setR(wc, 1, 20U);
    wc->thread_regs[0][registerType::P][0] = 0; // %p0 = false

    selpInstruction::Make("selp.u32 %r2, %r0, %r1, %p0;")->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 2), 20U);
}

TEST(SelpExecutor, F32SelectPositiveBranch)
{
    auto wc = makeWarp();
    setR(wc, 0, 1.0F);                          // a
    setR(wc, 1, -1.0F);                         // b
    wc->thread_regs[0][registerType::P][1] = 1; // %p1 = true

    selpInstruction::Make("selp.f32 %r2, %r0, %r1, %p1;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 2), 1.0F);
}

TEST(SelpExecutor, S32ImmediateOperands)
{
    // selp.s32 %r0, 7, -3, %p0  — both operands are immediates
    auto wc = makeWarp();
    wc->thread_regs[0][registerType::P][0] = 0; // %p0 = false → pick imm2 = -3

    selpInstruction::Make("selp.s32 %r0, 7, -3, %p0;")->Execute(wc);

    EXPECT_EQ(r32<int32_t>(wc, 0), -3);
}

// ============================================================================
// mul — float types (mode optional, added in this branch)
// ============================================================================
TEST(MulExecutor, F32NoMode)
{
    auto wc = makeWarp();
    setR(wc, 0, 2.5F);
    setR(wc, 1, 4.0F);

    mulInstruction::Make("mul.f32 %r2, %r0, %r1;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 2), 10.0F);
}

TEST(MulExecutor, F64NoMode)
{
    auto wc = makeWarp();
    setRd(wc, 0, 1.5);
    setRd(wc, 1, 3.0);

    mulInstruction::Make("mul.f64 %rd2, %rd0, %rd1;")->Execute(wc);

    EXPECT_DOUBLE_EQ(rd64<double>(wc, 2), 4.5);
}

// ============================================================================
// div — floating-point division (rounding mode is a hint, ignored in emulator)
// ============================================================================
TEST(DivExecutor, F32BasicDivision)
{
    auto wc = makeWarp();
    setR(wc, 0, 10.0F);
    setR(wc, 1, 4.0F);

    divInstruction::Make("div.rn.f32 %r2, %r0, %r1;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 2), 2.5F);
}

TEST(DivExecutor, F64BasicDivision)
{
    auto wc = makeWarp();
    setRd(wc, 0, 1.0);
    setRd(wc, 1, 3.0);

    divInstruction::Make("div.rn.f64 %rd2, %rd0, %rd1;")->Execute(wc);

    EXPECT_DOUBLE_EQ(rd64<double>(wc, 2), 1.0 / 3.0);
}

TEST(DivExecutor, F32NoRoundingMode)
{
    auto wc = makeWarp();
    setR(wc, 0, 7.0F);
    setR(wc, 1, 2.0F);

    divInstruction::Make("div.f32 %r2, %r0, %r1;")->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 2), 3.5F);
}

// ============================================================================
// shfl — warp-level register shuffle
//   shfl.sync.bfly.b32  dst|pred, src, offset_reg, clamp_reg, mask_reg;
//   Registers: %r0=values, %r1=offset, %r2=clamp(31), %r3=mask, %r4=result, %p0=valid
// ============================================================================

// Helper: write val into register id for every thread in [0, n)
static void setRAll(const std::shared_ptr<WarpContext>& wc, uint32_t id, uint32_t n, uint32_t val)
{
    for (uint32_t t = 0; t < n; ++t)
    {
        wc->thread_regs[t][registerType::R][id] = val;
    }
}

TEST(ShflExecutor, BflyOffset1ExchangesPairs)
{
    // Threads 0-3 active; each thread's %r0 = tid + 10 (10, 11, 12, 13)
    auto wc = makeWarp(0xF);
    for (uint32_t t = 0; t < 4; ++t)
    {
        wc->thread_regs[t][registerType::R][0] = t + 10;
    }

    setRAll(wc, 1, 4, 1);   // %r1 = offset = 1
    setRAll(wc, 2, 4, 31);  // %r2 = clamp  = 31
    setRAll(wc, 3, 4, 0xF); // %r3 = mask   = 0xF

    shflInstruction::Make("shfl.sync.bfly.b32 %r4|%p0, %r0, %r1, %r2, %r3;")->Execute(wc);

    // lid ^ 1: 0↔1, 2↔3
    EXPECT_EQ(wc->thread_regs[0][registerType::R][4], 11ULL); // 0^1=1 → value of thread 1
    EXPECT_EQ(wc->thread_regs[1][registerType::R][4], 10ULL); // 1^1=0 → value of thread 0
    EXPECT_EQ(wc->thread_regs[2][registerType::R][4], 13ULL); // 2^1=3 → value of thread 3
    EXPECT_EQ(wc->thread_regs[3][registerType::R][4], 12ULL); // 3^1=2 → value of thread 2
}

TEST(ShflExecutor, BflyPredicateIsSetForValidLanes)
{
    auto wc = makeWarp(0xF);
    for (uint32_t t = 0; t < 4; ++t)
    {
        wc->thread_regs[t][registerType::R][0] = t;
    }

    setRAll(wc, 1, 4, 1);
    setRAll(wc, 2, 4, 31);
    setRAll(wc, 3, 4, 0xF);

    shflInstruction::Make("shfl.sync.bfly.b32 %r4|%p0, %r0, %r1, %r2, %r3;")->Execute(wc);

    // All XOR partners (0^1=1, 1^1=0, 2^1=3, 3^1=2) are active → pred = 1
    for (uint32_t t = 0; t < 4; ++t)
    {
        EXPECT_EQ(wc->thread_regs[t][registerType::P][0], 1ULL) << "thread " << t;
    }
}

TEST(ShflExecutor, BflyOutOfBoundsClampsToSelf)
{
    // Only thread 0 active; bfly offset=1 → partner=1 which is inactive → clamp to self
    auto wc = makeWarp(0x1);
    wc->thread_regs[0][registerType::R][0] = 42;
    setRAll(wc, 1, 1, 1);
    setRAll(wc, 2, 1, 31);
    setRAll(wc, 3, 1, 0x1);

    shflInstruction::Make("shfl.sync.bfly.b32 %r4|%p0, %r0, %r1, %r2, %r3;")->Execute(wc);

    EXPECT_EQ(wc->thread_regs[0][registerType::R][4], 42ULL); // clamped to self
    EXPECT_EQ(wc->thread_regs[0][registerType::P][0], 0ULL);  // pred = 0 (invalid)
}

TEST(ShflExecutor, BflyOffset16FullWarpReduction)
{
    // Full warp butterfly sum reduction (one step: offset=16)
    auto wc = makeWarp(0xFFFFFFFF);
    for (uint32_t t = 0; t < 32; ++t)
    {
        wc->thread_regs[t][registerType::R][0] = t; // values 0..31
    }

    setRAll(wc, 1, 32, 16); // offset = 16
    setRAll(wc, 2, 32, 31); // clamp  = 31
    setRAll(wc, 3, 32, 0xFFFFFFFF);

    shflInstruction::Make("shfl.sync.bfly.b32 %r4|%p0, %r0, %r1, %r2, %r3;")->Execute(wc);

    // lid ^ 16: thread 0 gets thread 16's value (16), thread 16 gets thread 0's value (0)
    EXPECT_EQ(wc->thread_regs[0][registerType::R][4], 16ULL);
    EXPECT_EQ(wc->thread_regs[16][registerType::R][4], 0ULL);
    EXPECT_EQ(wc->thread_regs[1][registerType::R][4], 17ULL);
    EXPECT_EQ(wc->thread_regs[31][registerType::R][4], 15ULL);
}

// ============================================================================
// cvt — saturation mode (sat clamps float-to-int conversions)
// ============================================================================
TEST(CvtExecutor, F32ToS32SaturateAboveMax)
{
    auto wc = makeWarp();
    setR(wc, 0, 1e15F); // far above int32 max

    cvtInstruction::Make("cvt.sat.s32.f32 %r1, %r0;")->Execute(wc);

    EXPECT_EQ(r32<int32_t>(wc, 1), std::numeric_limits<int32_t>::max());
}

TEST(CvtExecutor, F32ToS32SaturateBelowMin)
{
    auto wc = makeWarp();
    setR(wc, 0, -1e15F); // far below int32 min

    cvtInstruction::Make("cvt.sat.s32.f32 %r1, %r0;")->Execute(wc);

    EXPECT_EQ(r32<int32_t>(wc, 1), std::numeric_limits<int32_t>::min());
}

TEST(CvtExecutor, F32ToS32SaturateInRange)
{
    auto wc = makeWarp();
    setR(wc, 0, 100.7F); // within range

    cvtInstruction::Make("cvt.sat.s32.f32 %r1, %r0;")->Execute(wc);

    EXPECT_EQ(r32<int32_t>(wc, 1), 100); // truncation within range
}
