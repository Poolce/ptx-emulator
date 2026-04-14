#include "global_context.h"

#ifdef EMULATOR_OPENMP_ENABLED
    #include <omp.h>
#endif

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

    size_t total_blocks = (size_t)gridDim.x * gridDim.y * gridDim.z;
    blocks_.resize(total_blocks);
    auto self = shared_from_this();

#ifdef EMULATOR_OPENMP_ENABLED
    #pragma omp parallel for collapse(3) schedule(dynamic)
#endif
    for (uint32_t gidz = 0; gidz < gridDim.z; ++gidz)
    {
        for (uint32_t gidy = 0; gidy < gridDim.y; ++gidy)
        {
            for (uint32_t gidx = 0; gidx < gridDim.x; ++gidx)
            {
                size_t idx = (size_t)gidz * gridDim.y * gridDim.x + gidy * gridDim.x + gidx;
                dim3 gid{gidx, gidy, gidz};
                auto bc = std::make_shared<BlockContext>();
                bc->Init(self, gridDim, gid, blockDim, sharedMem);
                blocks_[idx] = std::move(bc);
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

#ifdef EMULATOR_OPENMP_ENABLED
    #pragma omp parallel for schedule(static)
#endif
    for (size_t i = 0; i < blocks_.size(); ++i)
    {
        for (const auto& warp : blocks_[i]->GetWarps())
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

void* GlobalContext::GetParamPtr(const std::string& name) const
{
    auto it = global_parameters_.find(name);
    if (it == global_parameters_.end())
    {
        throw std::runtime_error("Unknown kernel parameter: " + name);
    }
    return args_[it->second.id];
}

} // namespace Emulator
