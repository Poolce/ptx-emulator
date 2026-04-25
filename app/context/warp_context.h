#pragma once

#include "profiler.h"
#include "ptx_types.h"
#include "types.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

namespace Emulator
{

using RegisterContext = std::vector<uint64_t>;
using ThreadContext = std::unordered_map<Ptx::registerType, RegisterContext>;
using SprContext = std::unordered_map<Ptx::sprType, uint64_t>;

class BlockContext;

// One entry on the PDOM divergence stack.
//   is_convergence=false : a diverged path waiting to be executed (pc, mask)
//   is_convergence=true  : an IPDOM reconvergence point; when execution reaches
//                          pc, restore execution_mask to mask and pop this frame
struct StackFrame
{
    uint64_t pc = 0;
    uint32_t mask = 0;
    bool is_convergence = false;
};

class WarpContext
{
  private:
    std::weak_ptr<BlockContext> block_context_;

  public:
    // Execution Context
    std::string cur_function;
    uint64_t pc = 0;
    uint32_t execution_mask = 0x0;
    std::stack<StackFrame> execution_stack;

    // Register Context
    std::vector<ThreadContext> thread_regs;
    std::vector<SprContext> spr_regs;

    // Profiling
    uint32_t warp_id = 0;
    WarpProfilingBuffer* profiling_buf = nullptr;

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
    uint64_t GetBasicBlockPc(const std::string& sym) const;
    void* getParamPtr(const std::string& name);
    void registerSharedSymbol(const std::string& name, size_t size, size_t align);
    void* getSharedPtr(const std::string& name);
    void* getSharedBase();

    dim3 GetBlockId() const;
    std::string GetBasicBlockAt(uint64_t pc) const;
};

} // namespace Emulator
