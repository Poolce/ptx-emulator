#include "rt_stream.h"

#ifdef EMULATOR_OPENMP_ENABLED
    #include <omp.h>
#endif

#include <chrono>

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
    for (auto& blockAllocator : gpu_context_->GetBlocks())
    {
        BlockContext block = blockAllocator();

        const auto& warps = block.GetWarps();

#ifdef EMULATOR_OPENMP_ENABLED
    #pragma omp parallel for
#endif
        for (size_t i = 0; i < warps.size(); ++i) // NOLINT(modernize-loop-convert)
        {
            auto warp = warps[i];
            while (warp->isActive())
            {
                auto instr = gpu_context_->GetInstruction(warp->pc);
                if (!instr)
                {
                    throw std::runtime_error("No instruction at pc " + std::to_string(warp->pc));
                }
                instr->Execute(warp);
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Execution time: " << elapsed.count() << " sec\n";
}

} // namespace Emulator
