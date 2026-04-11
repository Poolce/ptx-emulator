#include "block_context.h"

#include "constant.h"
#include "global_context.h"

namespace Emulator
{

void BlockContext::Init(const std::shared_ptr<GlobalContext>& global_context,
                        const dim3& gridDim,
                        const dim3& gridId,
                        const dim3& blockDim,
                        size_t sharedMem)
{
    global_context_ = global_context;
    shared_memory_ = std::vector<uint8_t>(sharedMem);
    std::vector<dim3> warp_thread_ids;
    warp_thread_ids.reserve(WarpSize);

    for (uint32_t tidz = 0; tidz < blockDim.z; ++tidz)
    {
        for (uint32_t tidy = 0; tidy < blockDim.y; ++tidy)
        {
            for (uint32_t tidx = 0; tidx < blockDim.x; ++tidx)
            {
                warp_thread_ids.emplace_back(tidx, tidy, tidz);
                if (warp_thread_ids.size() == WarpSize)
                {
                    auto warp = std::make_shared<WarpContext>();
                    warp->Init(shared_from_this(), gridDim, gridId, blockDim, warp_thread_ids);
                    warps_.push_back(warp);
                    warp_thread_ids.clear();
                }
            }
        }
    }
    if (!warp_thread_ids.empty())
    {
        auto warp = std::make_shared<WarpContext>();
        warp->Init(shared_from_this(), gridDim, gridId, blockDim, warp_thread_ids);
        warps_.push_back(warp);
    }
}

std::vector<std::shared_ptr<WarpContext>> BlockContext::GetWarps() const
{
    return warps_;
}

uint64_t BlockContext::GetBasicBlockOffset(const std::string& func_name, const std::string& sym) const
{
    auto global_context = global_context_.lock();
    if (!global_context)
    {
        throw std::runtime_error("Global context is expired.");
    }
    return global_context->GetBasicBlockOffset(func_name, sym);
}

void* BlockContext::GetParamPtr(const std::string& name) const
{
    auto global_context = global_context_.lock();
    if (!global_context)
    {
        throw std::runtime_error("Global context is expired.");
    }
    return global_context->GetParamPtr(name);
}

} // namespace Emulator