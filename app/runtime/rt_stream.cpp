#include "rt_stream.h"

namespace Emulator
{

RtStream::RtStream(const std::shared_ptr<Ptx::Module>& module)
{
    ptx_module_ = module;
}

void RtStream::KernelLaunch(const std::string& func, dim3 gridDim, dim3 blockDim, void** args, size_t sharedMem)
{
    gpu_context_ = std::make_shared<GlobalContext>();
    gpu_context_->Init(ptx_module_, gridDim, blockDim, args, sharedMem);
    gpu_context_->SetEntryFunction(func);

    for (auto& block : gpu_context_->GetBlocks())
    {
        for (auto& warp : block->GetWarps())
        {
            while (warp->isActive())
            {
                auto instr = gpu_context_->GetInstruction(warp->pc);
                instr->Execute(warp);
            }
        }
    }
}

} // namespace Emulator
