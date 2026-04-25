#include "rt_stream.h"

#include "logger.h"
#include "profiler.h"

#include <chrono>

#ifdef EMULATOR_OPENMP_ENABLED
    #include <omp.h>
#endif

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

            // Allocate profiling buffers once per warp before any parallel execution.
            std::vector<WarpProfilingBuffer> prof_bufs(warps.size());
            if (Profiler::instance().IsEnabled())
            {
                for (size_t i = 0; i < warps.size(); ++i)
                {
                    warps[i]->profiling_buf = &prof_bufs[i];
                }
            }

#ifdef EMULATOR_OPENMP_ENABLED
            // OMP path: one thread per warp, bar.sync implemented via BlockBarrier.
            block->InitBarrier(static_cast<int>(warps.size()));
            const int n_warps = static_cast<int>(warps.size());
            std::exception_ptr first_ex;

    #pragma omp parallel for schedule(static, 1) num_threads(n_warps)
            for (int i = 0; i < n_warps; ++i)
            {
                auto warp = warps[static_cast<size_t>(i)];
                try
                {
                    while (warp->isActive())
                    {
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
                }
                catch (...)
                {
    #pragma omp critical
                    {
                        if (!first_ex)
                        {
                            first_ex = std::current_exception();
                        }
                    }
                }
                // Always signal done so that other warps blocked in bar.sync are unblocked.
                block->WarpDone();
            }

            if (first_ex)
            {
                std::rethrow_exception(first_ex);
            }
#else
            bool any_active = true;
            while (any_active)
            {
                any_active = false;
                for (size_t i = 0; i < warps.size(); ++i)
                {
                    auto warp = warps[i];
                    if (!warp->isActive())
                    {
                        continue;
                    }
                    any_active = true;

                    while (warp->isActive())
                    {
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
                        const bool is_barrier = (instr->Name() == "bar");
                        instr->Execute(warp);

                        if (is_barrier)
                        {
                            break;
                        }
                    }
                }
            }
#endif

            if (Profiler::instance().IsEnabled())
            {
                for (size_t i = 0; i < warps.size(); ++i)
                {
                    Profiler::instance().Flush(prof_bufs[i]);
                    warps[i]->profiling_buf = nullptr;
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
