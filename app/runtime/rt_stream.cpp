#include "rt_stream.h"

#include "logger.h"
#include "profiler.h"

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
    LOG_INFO("Kernel '" + func + "' start" + " grid=(" + std::to_string(gridDim.x) + "," + std::to_string(gridDim.y) +
             "," + std::to_string(gridDim.z) + ")" + " block=(" + std::to_string(blockDim.x) + "," +
             std::to_string(blockDim.y) + "," + std::to_string(blockDim.z) + ")");

    try
    {
        auto gpu_context = std::make_shared<GlobalContext>();
        gpu_context->Init(ptx_module_, gridDim, blockDim, args, sharedMem);
        gpu_context->SetEntryFunction(func);

        if (Profiler::instance().IsEnabled())
        {
            Profiler::instance().BeginLaunch(func);
        }

        auto start = std::chrono::high_resolution_clock::now();
        for (auto& block : gpu_context->GetBlocks())
        {
            const auto& warps = block->GetWarps();
#ifdef EMULATOR_OPENMP_ENABLED
    #pragma omp parallel for
#endif
            for (size_t i = 0; i < warps.size(); ++i) // NOLINT(modernize-loop-convert)
            {
                auto warp = warps[i];
                WarpProfilingBuffer prof_buf;
                if (Profiler::instance().IsEnabled())
                {
                    warp->profiling_buf = &prof_buf;
                }
                while (warp->isActive())
                {
                    // PDOM reconvergence: if the taken (if) path reached the IPDOM
                    // by falling through (no explicit bra), restore the merged mask.
                    while (!warp->execution_stack.empty() && warp->execution_stack.top().is_convergence &&
                           warp->execution_stack.top().pc == warp->pc)
                    {
                        warp->execution_mask = warp->execution_stack.top().mask;
                        warp->execution_stack.pop();
                    }

                    auto instr = gpu_context->GetInstruction(warp->pc);
                    if (!instr)
                    {
                        throw std::runtime_error("No instruction at pc " + std::to_string(warp->pc));
                    }
                    LOG_DEBUG("Execute " + std::string(instr->Name()) + " at pc=" + std::to_string(warp->pc));
                    instr->Execute(warp);
                }
                if (Profiler::instance().IsEnabled())
                {
                    Profiler::instance().Flush(prof_buf);
                    warp->profiling_buf = nullptr;
                }
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        LOG_INFO("Kernel '" + func + "' done in " + std::to_string(elapsed.count()) + " sec");
    }
    catch (const std::runtime_error& e)
    {
        LOG_ERROR("Kernel '" + func + "' failed: " + e.what());
        throw;
    }
}

} // namespace Emulator
