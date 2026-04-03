#include "rt_stream.h"

namespace Emulator
{

RtStream::RtStream(const std::shared_ptr<Ptx::Module>& module)
{
    execution_module_ = std::make_shared<ExecutionModule>(module);
}

void RtStream::KernelLaunch(const std::string& func, dim3 gridDim, dim3 blockDim, void** args, size_t sharedMem)
{
    context_ = std::make_shared<GlobalContext>(gridDim, blockDim, args, sharedMem);

    for (auto& block : context_->GetBlocks())
    {
        for (auto& warp : block->GetWarps())
        {
            execution_module_->SetEntryFunction(warp, func);
            auto instr = execution_module_->GetInstruction(warp);
            while (instr)
            {
                instr->Execute(warp);
                instr = execution_module_->GetInstruction(warp);
            }
        }
    }
}

} // namespace Emulator
