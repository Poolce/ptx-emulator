#include "rt_stream.h"

namespace Emulator
{

RtStream::RtStream(const std::shared_ptr<Ptx::Module>& module)
{
    execution_module_ = std::make_shared<ExecutionModule>(module);
}

void RtStream::KernelLaunch([[maybe_unused]] uint64_t func,
                            [[maybe_unused]] dim3 gridDim,
                            [[maybe_unused]] dim3 blockDim,
                            [[maybe_unused]] void** args,
                            [[maybe_unused]] size_t sharedMem)
{
    context_ = std::make_shared<GlobalContext>(gridDim, blockDim, args, sharedMem);
}

} // namespace Emulator
