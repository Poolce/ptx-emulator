// AUTO-GENERATED METRIC IMPLEMENTATIONS — edit this file to add new metric computations.
// New metrics require: (1) entry in profiling_metrics.json, (2) ComputeXxx function here.
#pragma once

#include "profiler.h"

#include <algorithm>
#include <bit>
#include <iomanip>
#include <sstream>
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

// ---------------------------------------------------------------------------
// branch_efficiency
// For non-branch instructions all active threads execute the same code path,
// so efficiency is 1.0.  For bra, after ExecuteBranch() wc->execution_mask
// holds the threads that took the branch; orig_mask was the pre-branch mask.
// Efficiency = max(taken, not_taken) / total_active.
// ---------------------------------------------------------------------------
inline float ComputeBranchEfficiency(const Instruction& /*instr*/,
                                     uint32_t orig_mask,
                                     const std::shared_ptr<WarpContext>& wc)
{
    const auto active = static_cast<uint32_t>(std::popcount(orig_mask));
    if (active == 0)
    {
        return 1.0f;
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

} // namespace Profiling
} // namespace Ptx
} // namespace Emulator
