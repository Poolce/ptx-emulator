#include "warp_context.h"

#include "block_context.h"
#include "gpu_config.h"

#include <bit>
#include <iostream>

namespace Emulator
{

void WarpContext::Init(const std::shared_ptr<BlockContext>& block_context,
                       [[maybe_unused]] const dim3& gridDim,
                       const dim3& gridId,
                       const dim3& blockDim,
                       const std::vector<dim3>& tids)
{
    block_context_ = block_context;
    const uint32_t ws = GpuConfig::instance().warp_size;
    thread_regs = std::vector<ThreadContext>(ws);
    spr_regs = std::vector<SprContext>(ws);

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
    for (uint32_t i = 0; i < GpuConfig::instance().warp_size; ++i)
    {
        bool prd = false;
        if (i < thread_regs.size())
        {
            auto it = thread_regs[i].find(Ptx::registerType::P);
            if (it != thread_regs[i].end() && prd_id < it->second.size())
            {
                prd = it->second[prd_id] != 0;
            }
        }
        if (prd)
        {
            mask |= (1U << i);
        }
    }
    return mask;
}

bool WarpContext::isActive() const
{
    return !execution_stack.empty() || pc != EOC;
}

void WarpContext::gotoBasicBlock(const std::string& sym)
{
    auto block_context = block_context_.lock();
    if (!block_context)
    {
        throw std::runtime_error("Block context is expired.");
    }
    pc = block_context->GetBasicBlockOffset(cur_function, sym);
}

uint64_t WarpContext::GetBasicBlockPc(const std::string& sym) const
{
    auto block_context = block_context_.lock();
    if (!block_context)
    {
        return EOC;
    }
    return block_context->GetBasicBlockOffset(cur_function, sym);
}

void* WarpContext::getParamPtr(const std::string& name)
{
    auto block_context = block_context_.lock();
    if (!block_context)
    {
        throw std::runtime_error("Block context is expired.");
    }
    void* shared_ptr = block_context->GetSharedPtr(name);
    if (shared_ptr)
    {
        auto* base = static_cast<uint8_t*>(block_context->GetSharedBase());
        auto offset = static_cast<uintptr_t>(static_cast<uint8_t*>(shared_ptr) - base);
        return std::bit_cast<void*>(offset);
    }
    return block_context->GetParamPtr(name);
}

void WarpContext::registerSharedSymbol(const std::string& name, size_t size, size_t align)
{
    auto block_context = block_context_.lock();
    if (!block_context)
    {
        throw std::runtime_error("Block context is expired.");
    }
    block_context->RegisterSharedSymbol(name, size, align);
}

void* WarpContext::getSharedPtr(const std::string& name)
{
    auto block_context = block_context_.lock();
    if (!block_context)
    {
        throw std::runtime_error("Block context is expired.");
    }
    return block_context->GetSharedPtr(name);
}

void* WarpContext::getSharedBase()
{
    auto block_context = block_context_.lock();
    if (!block_context)
    {
        throw std::runtime_error("Block context is expired.");
    }
    return block_context->GetSharedBase();
}

dim3 WarpContext::GetBlockId() const
{
    if (spr_regs.empty())
    {
        return dim3{0, 0, 0};
    }
    const auto& lane0 = spr_regs[0];
    auto get = [&](Ptx::sprType t) -> uint32_t
    {
        auto it = lane0.find(t);
        return it != lane0.end() ? static_cast<uint32_t>(it->second) : 0U;
    };
    return dim3{get(Ptx::sprType::CtaidX), get(Ptx::sprType::CtaidY), get(Ptx::sprType::CtaidZ)};
}

std::string WarpContext::GetBasicBlockAt(uint64_t target_pc) const
{
    auto block_context = block_context_.lock();
    if (!block_context)
    {
        return "";
    }
    return block_context->GetBasicBlockAt(cur_function, target_pc);
}

} // namespace Emulator