#pragma once

#include "block_context.h"

namespace Emulator
{

class GlobalContext
{
  private:
    std::vector<std::shared_ptr<BlockContext>> blocks_;

  public:
    GlobalContext(const dim3& gridDim, const dim3& blockDim, void** args, size_t sharedMem);

    std::vector<std::shared_ptr<BlockContext>> GetBlocks() const;
};

} // namespace Emulator
