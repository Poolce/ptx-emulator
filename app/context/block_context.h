#pragma once

#include "warp_context.h"

namespace Emulator
{

class GlobalContext;

class BlockContext : public std::enable_shared_from_this<BlockContext>
{
  private:
    std::weak_ptr<GlobalContext> global_context_;

    std::vector<std::shared_ptr<WarpContext>> warps_;
    std::vector<uint8_t> shared_memory_;

  public:
    BlockContext() = default;

    void Init(std::shared_ptr<GlobalContext> global_context,
              const dim3& gridDim,
              const dim3& gridId,
              const dim3& blockDim,
              size_t sharedMem);
    std::vector<std::shared_ptr<WarpContext>> GetWarps() const;
};

} // namespace Emulator
