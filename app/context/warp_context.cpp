#include "warp_context.h"

#include "constant.h"

namespace Emulator
{

WarpContext::WarpContext([[maybe_unused]] const dim3& gridDim,
                         [[maybe_unused]] const dim3& gridId,
                         [[maybe_unused]] const dim3& blockDim,
                         [[maybe_unused]] const std::vector<dim3>& tids,
                         [[maybe_unused]] void** args,
                         [[maybe_unused]] void* shared_memory)
{
    thread_regs = std::vector<ThreadContext>(WARP_SIZE);
    spr_regs = std::vector<SprContext>(WARP_SIZE);
    args_ = args;
    shared_memory_ = shared_memory;
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

} // namespace Emulator