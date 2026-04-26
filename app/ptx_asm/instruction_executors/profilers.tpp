// AUTO-GENERATED METRIC IMPLEMENTATIONS — edit this file to add new metric computations.
// New metrics require: (1) entry in profiling_metrics.json, (2) ComputeXxx function here.
#pragma once

#include "profiler.h"
#include "gpu_config.h"

#include <algorithm>
#include <bit>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace Emulator {
namespace Ptx {
namespace Profiling {

inline std::string FormatFloat(float v)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << v;
    return oss.str();
}

inline float ComputeBranchEfficiency(const Instruction& instr,
                                     uint32_t orig_mask,
                                     const std::shared_ptr<WarpContext>& wc)
{
    const auto warp_size = static_cast<uint32_t>(GpuConfig::instance().warp_size);
    const auto active = static_cast<uint32_t>(std::popcount(orig_mask));
    if (active == 0)
    {
        return 0.0f;
    }
    if (!instr.HasExplicitPredicate())
    {
        // No explicit predicate: inactive lanes are idle due to prior divergence
        return static_cast<float>(active) / static_cast<float>(warp_size);
    }
    const auto on_path = static_cast<uint32_t>(std::popcount(wc->execution_mask & orig_mask));
    const auto off_path = active - on_path;
    return static_cast<float>(std::max(on_path, off_path)) / static_cast<float>(active);
}

// ---------------------------------------------------------------------------
// bank_conflicts
// Applies only to ld instructions targeting shared memory.  A conflict occurs
// when multiple threads in a warp access different addresses that map to the
// same 32-bit bank (bank = (addr / 4) % 32).
// Returns: max threads hitting a single bank minus 1 (0 = no conflicts).
// ---------------------------------------------------------------------------
inline int ComputeBankConflicts(const Instruction& instr,
                                uint32_t orig_mask,
                                const std::shared_ptr<WarpContext>& wc)
{
    const auto& ld = static_cast<const ldInstruction&>(instr);
    if (ld.space_ != ldspaceQl::Shared)
    {
        return 0;
    }

    const auto& cfg = Emulator::GpuConfig::instance();
    const uint32_t n_banks = cfg.bank_count;
    const uint32_t b_width = cfg.bank_width;
    std::vector<uint32_t> bank_hits(n_banks, 0u);
    for (uint32_t lid = 0; lid < cfg.warp_size; ++lid)
    {
        if (!((orig_mask >> lid) & 1U))
        {
            continue;
        }
        uintptr_t addr = 0;
        if (ld.addr_.reg.type != registerType::UNDEFINED)
        {
            addr = static_cast<uintptr_t>(
                wc->thread_regs[lid].at(ld.addr_.reg.type)[ld.addr_.reg.reg_id]);
        }
        addr += static_cast<uintptr_t>(static_cast<ptrdiff_t>(ld.addr_.imm));
        bank_hits[(addr / b_width) % n_banks]++;
    }

    const auto max_bank = *std::max_element(bank_hits.begin(), bank_hits.end());
    return max_bank > 1u ? static_cast<int>(max_bank - 1u) : 0;
}

// ---------------------------------------------------------------------------
// global_mem_transactions / global_coalescing
//
// For ld.global and st.global instructions, measures how many distinct
// 128-byte L1 cache lines are touched by the active threads in the warp
// (actual_transactions), and compares that to the theoretical minimum
// (ideal_transactions) assuming a perfectly packed sequential access pattern.
//
// Algorithm:
//   actual = |{ floor(addr_i / 128) : active thread i }|
//   ideal  = max(1, ceil(N × elem_bytes / 128))
//   efficiency = min(1.0, ideal / actual)
//
// Both functions re-read the address register from each thread's register
// file, which is safe because addr_ is a read-only source operand for both
// ld and st (only dst_ / the target memory location is written during
// ExecuteThread).
// ---------------------------------------------------------------------------

// Returns the element size in bytes for a dataType qualifier.
inline size_t DataTypeBytes(dataType dt)
{
    switch (dt)
    {
    case dataType::B8:  case dataType::U8:  case dataType::S8:               return 1;
    case dataType::B16: case dataType::U16: case dataType::S16: case dataType::F16: return 2;
    case dataType::B32: case dataType::U32: case dataType::S32: case dataType::F32: return 4;
    case dataType::B64: case dataType::U64: case dataType::S64: case dataType::F64: return 8;
    default:                                                                   return 4;
    }
}

// Gathers one virtual address per active thread for a global ld or st.
// Returns an empty vector for any non-global space qualifier.
inline std::vector<uintptr_t> CollectGlobalAddresses(
    const Instruction& instr, uint32_t orig_mask, const std::shared_ptr<WarpContext>& wc)
{
    const addressOperand* addr_field = nullptr;

    std::string_view name = instr.Name();
    if (name == "ld")
    {
        const auto& ld = static_cast<const ldInstruction&>(instr);
        if (ld.space_ != ldspaceQl::Global)
        {
            return {};
        }
        addr_field = &ld.addr_;
    }
    else if (name == "st")
    {
        const auto& st = static_cast<const stInstruction&>(instr);
        if (st.space_ != stspaceQl::Global)
        {
            return {};
        }
        addr_field = &st.addr_;
    }
    else
    {
        return {};
    }

    const auto& cfg = GpuConfig::instance();
    std::vector<uintptr_t> addrs;
    addrs.reserve(cfg.warp_size);

    for (uint32_t lid = 0; lid < cfg.warp_size; ++lid)
    {
        if (!((orig_mask >> lid) & 1U))
        {
            continue;
        }
        uintptr_t addr = 0;
        if (addr_field->reg.type != registerType::UNDEFINED)
        {
            addr = static_cast<uintptr_t>(
                wc->thread_regs[lid].at(addr_field->reg.type)[addr_field->reg.reg_id]);
        }
        addr += static_cast<uintptr_t>(static_cast<ptrdiff_t>(addr_field->imm));
        addrs.push_back(addr);
    }
    return addrs;
}

// Returns {actual_transactions, ideal_transactions} for the given address set.
inline std::pair<int, int> AnalyzeCoalescing(
    const std::vector<uintptr_t>& addrs, size_t elem_bytes)
{
    if (addrs.empty())
    {
        return {0, 0};
    }

    constexpr uint64_t CACHE_LINE_BYTES = 128;

    std::unordered_set<uint64_t> cache_lines;
    cache_lines.reserve(addrs.size());
    for (uintptr_t a : addrs)
    {
        cache_lines.insert(static_cast<uint64_t>(a) / CACHE_LINE_BYTES);
    }

    const int actual = static_cast<int>(cache_lines.size());
    const int ideal = std::max(1,
        static_cast<int>((addrs.size() * elem_bytes + CACHE_LINE_BYTES - 1) / CACHE_LINE_BYTES));
    return {actual, ideal};
}

// Returns the element size (bytes) for an ld or st instruction.
inline size_t InstrElemBytes(const Instruction& instr)
{
    std::string_view name = instr.Name();
    if (name == "ld")
    {
        return DataTypeBytes(GetParent(static_cast<const ldInstruction&>(instr).data_));
    }
    if (name == "st")
    {
        return DataTypeBytes(GetParent(static_cast<const stInstruction&>(instr).data_));
    }
    return 4;
}

// Metric: number of L1 cache-line transactions for a global memory access.
// Returns 0 for non-global accesses.
inline int ComputeGlobalMemTransactions(const Instruction& instr,
                                        uint32_t orig_mask,
                                        const std::shared_ptr<WarpContext>& wc)
{
    auto addrs = CollectGlobalAddresses(instr, orig_mask, wc);
    if (addrs.empty())
    {
        return 0;
    }
    return AnalyzeCoalescing(addrs, InstrElemBytes(instr)).first;
}

// Metric: coalescing efficiency = ideal_transactions / actual_transactions.
// 1.0 = perfectly coalesced. Returns 0.0 for non-global accesses.
inline float ComputeGlobalCoalescing(const Instruction& instr,
                                     uint32_t orig_mask,
                                     const std::shared_ptr<WarpContext>& wc)
{
    auto addrs = CollectGlobalAddresses(instr, orig_mask, wc);
    if (addrs.empty())
    {
        return 0.0f;
    }
    auto [actual, ideal] = AnalyzeCoalescing(addrs, InstrElemBytes(instr));
    if (actual == 0)
    {
        return 0.0f;
    }
    return std::min(1.0f, static_cast<float>(ideal) / static_cast<float>(actual));
}

} // namespace Profiling
} // namespace Ptx
} // namespace Emulator
