#pragma once

#include "ptx_types.h"

#include <cstdint>
#include <iterator>
#include <stack>
#include <unordered_map>
#include <vector>

namespace Emulator
{

using RegisterContext = std::vector<uint64_t>;
using ThreadContext = std::unordered_map<Ptx::registerType, RegisterContext>;
using SprContext = std::unordered_map<Ptx::sprType, uint64_t>;

struct WarpContext
{
    constexpr static uint64_t EOC = 0xffffffff;

    // Execution Context
    uint64_t pc = 0;
    uint32_t execution_mask = 0xffffffff;
    std::stack<std::pair<uint64_t, uint32_t>> execution_stack;
    std::unordered_map<std::string, uint8_t> global_parameters;

    // Register Context
    std::vector<ThreadContext> thread_regs{};
    std::vector<SprContext> spr_regs{};

    WarpContext();
};

} // namespace Emulator
