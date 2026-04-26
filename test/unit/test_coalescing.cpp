#include "instructions.h"
#include "warp_context.h"

#include <gtest/gtest.h>

#include <cstdint>

using namespace Emulator;
using namespace Emulator::Ptx;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static constexpr uint32_t WS = 32; // warp size used throughout

// Build a minimal WarpContext with 32-thread register file.
// Each thread gets its address in %r<addr_reg_id>.
static std::shared_ptr<WarpContext>
makeGlobalWarp(uint32_t mask, uint32_t addr_reg_id, const std::vector<uintptr_t>& addrs)
{
    auto wc = std::make_shared<WarpContext>();
    wc->execution_mask = mask;
    wc->thread_regs.resize(WS);
    wc->spr_regs.resize(WS);

    for (uint32_t lid = 0; lid < WS; ++lid)
    {
        wc->thread_regs[lid][registerType::R] = RegisterContext(8, 0);
        wc->spr_regs[lid][sprType::TidX] = lid;
    }

    // Plant the address for each active thread
    for (uint32_t lid = 0; lid < WS && lid < static_cast<uint32_t>(addrs.size()); ++lid)
    {
        wc->thread_regs[lid][registerType::R][addr_reg_id] = static_cast<uint64_t>(addrs[lid]);
    }
    return wc;
}

// Run Profile() on a freshly parsed instruction and return the metrics map.
static std::unordered_map<std::string, std::string>
profile(const std::string& ptx_line, std::shared_ptr<WarpContext> wc, uint32_t mask)
{
    auto instr = ldInstruction::Make(ptx_line);
    WarpProfilingBuffer buf;
    instr->Profile(wc, /*pc=*/0, mask, buf);

    std::unordered_map<std::string, std::string> result;
    if (!buf.empty())
    {
        for (const auto& [k, v] : buf[0].metrics)
        {
            result[k] = v;
        }
    }
    return result;
}

static std::unordered_map<std::string, std::string>
profile_st(const std::string& ptx_line, std::shared_ptr<WarpContext> wc, uint32_t mask)
{
    auto instr = stInstruction::Make(ptx_line);
    WarpProfilingBuffer buf;
    instr->Profile(wc, /*pc=*/0, mask, buf);

    std::unordered_map<std::string, std::string> result;
    if (!buf.empty())
    {
        for (const auto& [k, v] : buf[0].metrics)
        {
            result[k] = v;
        }
    }
    return result;
}

// ============================================================================
// AnalyzeCoalescing helpers (tested via instruction Profile)
// ============================================================================

// 32 threads loading consecutive 4-byte floats starting at a 128-byte aligned
// address: 32×4 = 128 bytes → exactly 1 cache line → efficiency = 1.0.
TEST(GlobalCoalescing, PerfectlyCoalesced32Threads)
{
    constexpr uintptr_t BASE = 0x1000; // 128-byte aligned

    std::vector<uintptr_t> addrs(WS);
    for (uint32_t i = 0; i < WS; ++i)
    {
        addrs[i] = BASE + i * 4; // consecutive f32 elements
    }

    const uint32_t mask = 0xFFFFFFFFu;
    auto wc = makeGlobalWarp(mask, /*addr_reg=*/0, addrs);
    auto m = profile("ld.global.f32 %r1, [%r0];", wc, mask);

    EXPECT_EQ(m.at("global_mem_transactions"), "1");
    EXPECT_EQ(m.at("global_coalescing"), "1.0000");
}

// 32 threads each touching a different 128-byte cache line (stride = 128 bytes).
// 32 actual transactions, ideal = ceil(32×4/128) = 1 → efficiency = 1/32.
TEST(GlobalCoalescing, FullyDivergent32Threads)
{
    constexpr uintptr_t BASE = 0x0;

    std::vector<uintptr_t> addrs(WS);
    for (uint32_t i = 0; i < WS; ++i)
    {
        addrs[i] = BASE + i * 128; // one per cache line
    }

    const uint32_t mask = 0xFFFFFFFFu;
    auto wc = makeGlobalWarp(mask, 0, addrs);
    auto m = profile("ld.global.f32 %r1, [%r0];", wc, mask);

    EXPECT_EQ(m.at("global_mem_transactions"), "32");
    // ideal=1, actual=32 → 1/32 ≈ 0.0313
    EXPECT_EQ(m.at("global_coalescing"), "0.0312");
}

// Only 4 active threads (mask = 0xF), consecutive f32 elements within one
// cache line → 1 transaction, efficiency = 1.0.
TEST(GlobalCoalescing, FourActiveThreadsCoalesced)
{
    constexpr uintptr_t BASE = 0x2000;

    std::vector<uintptr_t> addrs(WS, 0);
    for (uint32_t i = 0; i < 4; ++i)
    {
        addrs[i] = BASE + i * 4;
    }

    const uint32_t mask = 0xFu;
    auto wc = makeGlobalWarp(mask, 0, addrs);
    auto m = profile("ld.global.f32 %r1, [%r0];", wc, mask);

    EXPECT_EQ(m.at("global_mem_transactions"), "1");
    EXPECT_EQ(m.at("global_coalescing"), "1.0000");
}

// 32 threads loading f64 elements (8 bytes each) in consecutive order:
// 32×8 = 256 bytes → 2 cache lines → ideal = 2, actual = 2 → efficiency = 1.0.
TEST(GlobalCoalescing, F64PerfectlyCoalesced)
{
    constexpr uintptr_t BASE = 0x3000;

    std::vector<uintptr_t> addrs(WS);
    for (uint32_t i = 0; i < WS; ++i)
    {
        addrs[i] = BASE + i * 8;
    }

    const uint32_t mask = 0xFFFFFFFFu;
    auto wc = makeGlobalWarp(mask, 0, addrs);
    auto m = profile("ld.global.f64 %rd1, [%r0];", wc, mask);

    EXPECT_EQ(m.at("global_mem_transactions"), "2");
    EXPECT_EQ(m.at("global_coalescing"), "1.0000");
}

// Partially coalesced: 32 threads split across 2 cache lines (stride = 8 bytes,
// so 32×8 = 256 bytes → 2 lines). ideal=2, actual=2 → 1.0.
// But if stride = 16 (32×16=512 bytes → 4 lines), ideal still 2 → 0.5.
TEST(GlobalCoalescing, PartiallyCoalescedStride16)
{
    constexpr uintptr_t BASE = 0x4000; // 128-byte aligned

    std::vector<uintptr_t> addrs(WS);
    for (uint32_t i = 0; i < WS; ++i)
    {
        addrs[i] = BASE + i * 16; // stride 16 bytes: 32×16 = 512 bytes → 4 lines
    }

    const uint32_t mask = 0xFFFFFFFFu;
    auto wc = makeGlobalWarp(mask, 0, addrs);
    auto m = profile("ld.global.f32 %r1, [%r0];", wc, mask);

    // actual = 4 (512 bytes / 128 per line), ideal = ceil(32×4/128) = 1
    EXPECT_EQ(m.at("global_mem_transactions"), "4");
    EXPECT_EQ(m.at("global_coalescing"), "0.2500");
}

// All 32 threads access the SAME address (broadcast) → 1 transaction, ideal=1 → 1.0.
TEST(GlobalCoalescing, BroadcastAccess)
{
    constexpr uintptr_t ADDR = 0x5000;

    std::vector<uintptr_t> addrs(WS, ADDR);
    const uint32_t mask = 0xFFFFFFFFu;
    auto wc = makeGlobalWarp(mask, 0, addrs);
    auto m = profile("ld.global.u32 %r1, [%r0];", wc, mask);

    EXPECT_EQ(m.at("global_mem_transactions"), "1");
    EXPECT_EQ(m.at("global_coalescing"), "1.0000");
}

// ============================================================================
// Non-global accesses must not count transactions
// ============================================================================

// ld.shared must report 0 transactions and 0.0 coalescing (not applicable).
TEST(GlobalCoalescing, SharedMemoryReturnsZero)
{
    std::vector<uintptr_t> addrs(WS);
    for (uint32_t i = 0; i < WS; ++i)
    {
        addrs[i] = i * 4;
    }
    const uint32_t mask = 0xFFFFFFFFu;
    auto wc = makeGlobalWarp(mask, 0, addrs);
    auto m = profile("ld.shared.f32 %r1, [%r0];", wc, mask);

    EXPECT_EQ(m.at("global_mem_transactions"), "0");
    EXPECT_EQ(m.at("global_coalescing"), "0.0000");
}

// ============================================================================
// st.global coalescing
// ============================================================================

// Consecutive store pattern: efficiency 1.0.
TEST(GlobalCoalescing, StoreCoalesced)
{
    constexpr uintptr_t BASE = 0x6000;

    std::vector<uintptr_t> addrs(WS);
    for (uint32_t i = 0; i < WS; ++i)
    {
        addrs[i] = BASE + i * 4;
    }

    const uint32_t mask = 0xFFFFFFFFu;
    auto wc = makeGlobalWarp(mask, 0, addrs);
    // st.global uses addr_ as first operand
    auto m = profile_st("st.global.f32 [%r0], %r1;", wc, mask);

    EXPECT_EQ(m.at("global_mem_transactions"), "1");
    EXPECT_EQ(m.at("global_coalescing"), "1.0000");
}

// Divergent store: one cache line per thread.
TEST(GlobalCoalescing, StoreDivergent)
{
    std::vector<uintptr_t> addrs(WS);
    for (uint32_t i = 0; i < WS; ++i)
    {
        addrs[i] = i * 128;
    }

    const uint32_t mask = 0xFFFFFFFFu;
    auto wc = makeGlobalWarp(mask, 0, addrs);
    auto m = profile_st("st.global.f32 [%r0], %r1;", wc, mask);

    EXPECT_EQ(m.at("global_mem_transactions"), "32");
    EXPECT_EQ(m.at("global_coalescing"), "0.0312");
}

// ============================================================================
// Immediate offset: ld.global.f32 %r1, [%r0+16]
// All threads access the same base + 16 → same cache line → 1 transaction.
// ============================================================================
TEST(GlobalCoalescing, ImmediateOffset)
{
    constexpr uintptr_t BASE = 0x7000;
    // All threads share the same base address; the +16 offset is in the instruction.
    std::vector<uintptr_t> addrs(WS, BASE);
    const uint32_t mask = 0xFFFFFFFFu;
    auto wc = makeGlobalWarp(mask, 0, addrs);
    auto m = profile("ld.global.f32 %r1, [%r0+16];", wc, mask);

    EXPECT_EQ(m.at("global_mem_transactions"), "1");
    EXPECT_EQ(m.at("global_coalescing"), "1.0000");
}
