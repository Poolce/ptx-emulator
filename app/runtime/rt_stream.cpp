#include "rt_stream.h"

#include <chrono>
#include <omp.h>

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
    
    
    auto start = std::chrono::high_resolution_clock::now();
    for (auto& block : gpu_context_->GetBlocks())
    {
        const auto& warps = block->GetWarps();
        #pragma omp parallel for
        for (size_t i = 0; i < warps.size(); ++i)
        {
            auto warp = warps[i];
            while (warp->isActive())
            {
                auto instr = gpu_context_->GetInstruction(warp->pc);
                instr->Execute(warp);
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Execution time: " << elapsed.count() << " sec\n";
}

} // namespace Emulator
