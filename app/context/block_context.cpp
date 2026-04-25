#include "block_context.h"

#include "global_context.h"
#include "gpu_config.h"

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

    const auto& cfg = GpuConfig::instance();
    uint32_t total_threads = blockDim.x * blockDim.y * blockDim.z;
    uint32_t num_warps = (total_threads + cfg.warp_size - 1) / cfg.warp_size;
    warps_.reserve(num_warps);

    std::vector<dim3> warp_thread_ids;
    warp_thread_ids.reserve(cfg.warp_size);

    auto make_warp = [&]()
    {
        auto warp = std::make_shared<WarpContext>();
        warp->Init(shared_from_this(), gridDim, gridId, blockDim, warp_thread_ids);
        warp->warp_id = static_cast<uint32_t>(warps_.size());
        warps_.push_back(std::move(warp));
    };

    for (uint32_t tidz = 0; tidz < blockDim.z; ++tidz)
    {
        for (uint32_t tidy = 0; tidy < blockDim.y; ++tidy)
        {
            for (uint32_t tidx = 0; tidx < blockDim.x; ++tidx)
            {
                warp_thread_ids.emplace_back(tidx, tidy, tidz);
                if (warp_thread_ids.size() == cfg.warp_size)
                {
                    make_warp();
                    warp_thread_ids.clear();
                }
            }
        }
    }
    if (!warp_thread_ids.empty())
    {
        make_warp();
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
    std::lock_guard<std::mutex> lock(shared_mutex_);
    if (shared_symbols_.contains(name))
    {
        return; // already registered by another warp
    }
    if (align > 0)
    {
        shared_offset_ = (shared_offset_ + align - 1) & ~(align - 1);
    }
    size_t required = shared_offset_ + size;
    if (required > shared_memory_.size())
    {
        shared_memory_.resize(required);
    }
    shared_symbols_[name] = &shared_memory_[shared_offset_];
    shared_offset_ += size;
}

void* BlockContext::GetSharedPtr(const std::string& name) const
{
    auto it = shared_symbols_.find(name);
    return it != shared_symbols_.end() ? it->second : nullptr;
}

void* BlockContext::GetSharedBase() const
{
    return shared_memory_.empty() ? nullptr : const_cast<uint8_t*>(shared_memory_.data());
}

std::string BlockContext::GetBasicBlockAt(const std::string& func_name, uint64_t pc) const
{
    auto global_context = global_context_.lock();
    if (!global_context)
    {
        return "";
    }
    return global_context->GetBasicBlockAt(func_name, pc);
}

void BlockContext::InitBarrier(int n)
{
    block_barrier_ = std::make_unique<BlockBarrier>(n);
}

void BlockContext::SyncBarrier()
{
    if (block_barrier_)
    {
        block_barrier_->sync();
    }
}

void BlockContext::WarpDone()
{
    if (block_barrier_)
    {
        block_barrier_->warp_done();
    }
}

} // namespace Emulator