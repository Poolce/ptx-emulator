#include "global_context.h"

namespace Emulator
{

void GlobalContext::Init(const std::shared_ptr<Ptx::Module>& ptx,
                         const dim3& gridDim,
                         const dim3& blockDim,
                         void** args,
                         size_t sharedMem)
{
    ptx_module_ = ptx;
    args_ = args;
    for (uint32_t gidx = 0; gidx < gridDim.x; ++gidx)
    {
        for (uint32_t gidy = 0; gidy < gridDim.y; ++gidy)
        {
            for (uint32_t gidz = 0; gidz < gridDim.z; ++gidz)
            {
                dim3 gid{gidx, gidy, gidz};
                auto bc = std::make_shared<BlockContext>();
                bc->Init(shared_from_this(), gridDim, gid, blockDim, sharedMem);
                blocks_.push_back(bc);
            }
        }
    }
}

std::vector<std::shared_ptr<BlockContext>> GlobalContext::GetBlocks() const
{
    return blocks_;
}

void GlobalContext::SetEntryFunction(const std::string& func_name)
{
    auto func = ptx_module_->GetEntryFunction(func_name);
    auto pc = func->getOffset();
    global_parameters_ = func->getParameters();
    for (auto& block : blocks_)
    {
        for (auto warp : block->GetWarps())
        {
            warp->cur_function = func_name;
            warp->pc = pc;
        }
    }
}

std::shared_ptr<Ptx::Instruction> GlobalContext::GetInstruction(uint64_t pc) const
{
    return ptx_module_->GetInstruction(pc);
}

uint64_t GlobalContext::GetBasicBlockOffset(const std::string& func_name, const std::string& sym) const
{
    return ptx_module_->GetBasicBlockOffset(func_name, sym);
}

} // namespace Emulator
