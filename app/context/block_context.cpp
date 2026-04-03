#include "block_context.h"

#include "constant.h"

namespace Emulator
{

BlockContext::BlockContext(const dim3& gridDim, const dim3& gridId, const dim3& blockDim, void** args, size_t sharedMem)
{
    shared_memory_ = std::vector<uint8_t>(sharedMem);
    std::vector<dim3> warp_thread_ids;
    warp_thread_ids.reserve(WARP_SIZE);

    for (uint32_t tidz = 0; tidz < blockDim.z; ++tidz)
    {
        for (uint32_t tidy = 0; tidy < blockDim.y; ++tidy)
        {
            for (uint32_t tidx = 0; tidx < blockDim.x; ++tidx)
            {
                warp_thread_ids.push_back({tidx, tidy, tidz});
                if (warp_thread_ids.size() == WARP_SIZE)
                {
                    auto warp = std::make_shared<WarpContext>(gridDim,
                                                              gridId,
                                                              blockDim,
                                                              warp_thread_ids,
                                                              args,
                                                              shared_memory_.data());
                    warps_.push_back(warp);
                    warp_thread_ids.clear();
                }
            }
        }
    }
    if (warp_thread_ids.size() > 0)
    {
        auto warp =
            std::make_shared<WarpContext>(gridDim, gridId, blockDim, warp_thread_ids, args, shared_memory_.data());
        warps_.push_back(warp);
    }
}

std::vector<std::shared_ptr<WarpContext>> BlockContext::GetWarps() const
{
    return warps_;
}

} // namespace Emulator