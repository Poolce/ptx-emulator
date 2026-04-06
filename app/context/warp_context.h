#pragma once

#include "ptx_types.h"
#include "types.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>

namespace Emulator
{

using RegisterContext = std::vector<uint64_t>;
using ThreadContext = std::unordered_map<Ptx::registerType, RegisterContext>;
using SprContext = std::unordered_map<Ptx::sprType, uint64_t>;

class WarpContext
{
  public:
    // Execution Context
    uint64_t pc = 0;
    uint32_t execution_mask = 0x0;
    std::stack<std::pair<uint64_t, uint32_t>> execution_stack;
    std::unordered_map<std::string, Ptx::FunctionParameter> global_parameters;

    // Global Context
    void** args_;
    void* shared_memory_;

    // Register Context
    std::vector<ThreadContext> thread_regs{};
    std::vector<SprContext> spr_regs{};

  public:
    constexpr static uint64_t EOC = 0xffffffffffffffff;
    WarpContext(const dim3& gridDim,
                const dim3& gridId,
                const dim3& blockDim,
                const std::vector<dim3>& tids,
                void** args,
                void* shared_memory);
};

} // namespace Emulator
