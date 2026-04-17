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

    uint32_t total_threads = blockDim.x * blockDim.y * blockDim.z;
    uint32_t num_warps = (total_threads + WarpSize - 1) / WarpSize;
    warps_.reserve(num_warps);

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

    const auto &entry_name = global_context->GetEntryFunction().GetName();
    const auto pc = global_context->GetEntryFunction().getOffset();
#ifdef EMULATOR_OPENMP_ENABLED
    #pragma omp parallel for schedule(static)
#endif
    for (const auto& warp : GetWarps())
    {
        warp->cur_function = entry_name;
        warp->pc = pc;
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

void BlockContext::RegisterSharedSymbol(const std::string& name, size_t size, size_t align)
{
    if (shared_symbols_.contains(name))
    {
        return; // already registered by another warp
    }
    if (align > 0)
    {
        shared_offset_ = (shared_offset_ + align - 1) & ~(align - 1);
    }
    shared_symbols_[name] = &shared_memory_[shared_offset_];
    shared_offset_ += size;
}

void* BlockContext::GetSharedPtr(const std::string& name) const
{
    auto it = shared_symbols_.find(name);
    return it != shared_symbols_.end() ? it->second : nullptr;
}

} // namespace Emulator