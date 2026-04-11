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

class BlockContext;

class WarpContext
{
  private:
    std::weak_ptr<BlockContext> block_context_;

  public:
    // Execution Context
    std::string cur_function;
    uint64_t pc = 0;
    uint32_t execution_mask = 0x0;
    std::stack<std::pair<uint64_t, uint32_t>> execution_stack;

    // Register Context
    std::vector<ThreadContext> thread_regs;
    std::vector<SprContext> spr_regs;

    constexpr static uint64_t EOC = 0xffffffffffffffff;
    WarpContext() = default;
    void Init(const std::shared_ptr<BlockContext>& block_context,
              const dim3& gridDim,
              const dim3& gridId,
              const dim3& blockDim,
              const std::vector<dim3>& tids);

    bool isActive() const;

    uint32_t GetPredicateMask(uint64_t prd_id) const;
    void gotoBasicBlock(const std::string& sym);
    void* getParamPtr(const std::string& name);
};

} // namespace Emulator
