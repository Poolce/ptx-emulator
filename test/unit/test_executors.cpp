#include "constant.h"
#include "instructions.h"
#include "warp_context.h"

#include <gtest/gtest.h>

#include <bit>
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
    EXPECT_EQ(mask, 0x5u); // bits 0 and 2
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
    EXPECT_EQ(mask, 0x1u); // bit 0 set because %p0[0]=1
}

TEST(GetPredicateMask, OutOfBoundsPredicateIdReturnsFalse)
{
    auto wc = makeWarp(0x1);
    // prd_id = 99 is way beyond the 4-element P RegisterContext
    uint32_t mask = wc->GetPredicateMask(99);
    EXPECT_EQ(mask, 0u);
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

    EXPECT_EQ(wc->thread_regs[0][registerType::R].size(), 5u);
    EXPECT_EQ(wc->thread_regs[0][registerType::R][3], 0u);
    EXPECT_EQ(wc->pc, 1u);
}

// ============================================================================
// add
// ============================================================================
TEST(AddExecutor, U32RegisterPlusRegister)
{
    auto wc = makeWarp();
    setR(wc, 0, 10u);
    setR(wc, 1, 32u);

    auto instr = addInstruction::Make("add.u32 %r2, %r0, %r1;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 2), 42u);
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

    EXPECT_EQ(r32<uint32_t>(wc, 2), 0u); // wraps
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

    EXPECT_EQ(wc->thread_regs[0][registerType::R][2], 3u);
    EXPECT_EQ(wc->thread_regs[1][registerType::R][2], 300u);
    EXPECT_EQ(wc->thread_regs[2][registerType::R][2], 0u); // unchanged
}

// ============================================================================
// mov
// ============================================================================
TEST(MovExecutor, RegisterToRegister)
{
    auto wc = makeWarp();
    setR(wc, 0, 77u);

    auto instr = movInstruction::Make("mov.u32 %r1, %r0;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 1), 77u);
}

TEST(MovExecutor, ImmediateToRegister)
{
    auto wc = makeWarp();
    auto instr = movInstruction::Make("mov.u32 %r0, 42;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 0), 42u);
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

    EXPECT_EQ(wc->thread_regs[0][registerType::R][0], 0u);
    EXPECT_EQ(wc->thread_regs[1][registerType::R][0], 1u);
    EXPECT_EQ(wc->thread_regs[2][registerType::R][0], 2u);
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

    EXPECT_EQ(pred(wc, 0), 1u);
}

TEST(SetpExecutor, LtS32False)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(5));
    setR(wc, 1, int32_t(3));

    auto instr = setpInstruction::Make("setp.lt.s32 %p0 %r0 %r1;");
    instr->Execute(wc);

    EXPECT_EQ(pred(wc, 0), 0u);
}

TEST(SetpExecutor, EqU32)
{
    auto wc = makeWarp();
    setR(wc, 0, 7u);
    setR(wc, 1, 7u);

    auto instr = setpInstruction::Make("setp.eq.u32 %p0 %r0 %r1;");
    instr->Execute(wc);

    EXPECT_EQ(pred(wc, 0), 1u);
}

TEST(SetpExecutor, GeS32)
{
    auto wc = makeWarp();
    setR(wc, 0, int32_t(5));
    setR(wc, 1, int32_t(5));

    auto instr = setpInstruction::Make("setp.ge.s32 %p1 %r0 %r1;");
    instr->Execute(wc);

    EXPECT_EQ(pred(wc, 1), 1u);
}

TEST(SetpExecutor, NeU32)
{
    auto wc = makeWarp();
    setR(wc, 0, 1u);
    setR(wc, 1, 2u);

    auto instr = setpInstruction::Make("setp.ne.u32 %p0 %r0 %r1;");
    instr->Execute(wc);

    EXPECT_EQ(pred(wc, 0), 1u);
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

    EXPECT_EQ(r32<uint32_t>(wc, 1), 4u);
}

TEST(ShlExecutor, ShiftBy31)
{
    auto wc = makeWarp();
    setR(wc, 0, uint32_t(1));

    auto instr = shlInstruction::Make("shl.b32 %r1, %r0, 31;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 1), 0x80000000u);
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

    EXPECT_EQ(r32<uint32_t>(wc, 2), 0x0F000F00u);
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
    setR(wc, 0, 5.0f);
    setR(wc, 1, 3.0f);

    auto instr = subInstruction::Make("sub.f32 %r2, %r0, %r1;");
    instr->Execute(wc);

    EXPECT_FLOAT_EQ(r32<float>(wc, 2), 2.0f);
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

    auto instr = mulInstruction::Make("mul.hi.b32 %r2, %r0, %r1;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 2), 0xFFFFFFFEu);
}

// ============================================================================
// mad
// ============================================================================
TEST(MadExecutor, LoU32)
{
    auto wc = makeWarp();
    setR(wc, 0, 3u); // a
    setR(wc, 1, 4u); // b
    setR(wc, 2, 5u); // c

    auto instr = madInstruction::Make("mad.lo.u32 %r3, %r0, %r1, %r2;");
    instr->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 3), 17u); // 3*4+5
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

    EXPECT_EQ(mem, 42u);

    // ld.global.u32 %r1, [%rd0]  — load back
    auto ld = ldInstruction::Make("ld.global.u32 %r1, [%rd0];");
    ld->Execute(wc);

    EXPECT_EQ(r32<uint32_t>(wc, 1), 42u);
}

TEST(LdStExecutor, RoundtripF32)
{
    auto wc = makeWarp();
    float mem = 0.0f;

    setR(wc, 0, 3.14f);
    setRd(wc, 0, reinterpret_cast<uintptr_t>(&mem));

    stInstruction::Make("st.global.f32 [%rd0], %r0;")->Execute(wc);
    EXPECT_FLOAT_EQ(mem, 3.14f);

    ldInstruction::Make("ld.global.f32 %r1, [%rd0];")->Execute(wc);
    EXPECT_FLOAT_EQ(r32<float>(wc, 1), 3.14f);
}

TEST(LdStExecutor, StoreWithImmediateOffset)
{
    auto wc = makeWarp();
    uint32_t buf[4] = {0, 0, 0, 0};

    setR(wc, 0, uint32_t(99));
    setRd(wc, 0, reinterpret_cast<uintptr_t>(buf));

    // Store to buf[2] (offset = 8 bytes)
    stInstruction::Make("st.global.u32 [%rd0+8], %r0;")->Execute(wc);
    EXPECT_EQ(buf[0], 0u);
    EXPECT_EQ(buf[2], 99u);
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
    setR(wc, 0, 3.9f);

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

    EXPECT_FLOAT_EQ(r32<float>(wc, 1), 7.0f);
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
    wc->execution_stack.push({42u, 0x3u});

    auto instr = retInstruction::Make("ret;");
    instr->Execute(wc);

    EXPECT_EQ(wc->pc, 42u);
    EXPECT_EQ(wc->execution_mask, 0x3u);
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

    EXPECT_EQ(wc->pc, 6u);
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
    EXPECT_EQ(saved_pc, 11u);            // fall-through pc = 10+1
    EXPECT_EQ(saved_mask, 0x2u);         // bit 1 = fall-through thread
    EXPECT_EQ(wc->execution_mask, 0x1u); // only branching thread remains
}
