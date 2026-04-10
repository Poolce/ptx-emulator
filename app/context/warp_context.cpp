#include "block_context.h"
#include "warp_context.h"
#include "constant.h"

namespace Emulator
{

void WarpContext::Init(std::shared_ptr<BlockContext> block_context,
                       [[maybe_unused]] const dim3& gridDim,
                       const dim3& gridId,
                       const dim3& blockDim,
                       const std::vector<dim3>& tids)
{
    block_context_ = block_context;
    thread_regs = std::vector<ThreadContext>(WARP_SIZE);
    spr_regs = std::vector<SprContext>(WARP_SIZE);

    uint32_t tid_id = 0;
    for (const auto& tid : tids)
    {
        spr_regs[tid_id][Ptx::sprType::TidX] = tid.x;
        spr_regs[tid_id][Ptx::sprType::TidY] = tid.y;
        spr_regs[tid_id][Ptx::sprType::TidZ] = tid.z;
        spr_regs[tid_id][Ptx::sprType::CtaidX] = gridId.x;
        spr_regs[tid_id][Ptx::sprType::CtaidY] = gridId.y;
        spr_regs[tid_id][Ptx::sprType::CtaidZ] = gridId.z;
        spr_regs[tid_id][Ptx::sprType::NtidX] = blockDim.x;
        spr_regs[tid_id][Ptx::sprType::NtidY] = blockDim.y;
        spr_regs[tid_id][Ptx::sprType::NtidZ] = blockDim.z;
        tid_id++;
    }
    execution_mask = ((((uint64_t)1 << tid_id) - 1) & 0xffffffff);
}

uint32_t WarpContext::GetPredicateMask(uint64_t prd_id) const
{
    uint32_t mask = 0;
    for (uint32_t i = 0; i < WARP_SIZE; ++i)
    {
        bool prd = false;
        if (i > thread_regs.size())
        {
            prd = thread_regs.at(i).at(Ptx::registerType::P)[prd_id] != 0;
        }
        if (prd)
        {
            mask |= (1 << i);
        }
    }
    return mask;
}

bool WarpContext::isActive() const
{
    return !(execution_stack.empty() && pc == EOC);
}

void WarpContext::gotoBasicBlock(const std::string& sym)
{
    auto block_context = block_context_.lock();
    if (!block_context) {
        throw std::runtime_error("Block context is expired.");
    }
    pc = block_context->GetBasicBlockOffset(cur_function, sym);
}


} // namespace Emulator