#include "warp_context.h"

namespace Emulator
{

class BlockContext
{
  private:
    std::vector<std::shared_ptr<WarpContext>> warps_;
    std::vector<uint8_t> shared_memory_;

  public:
    BlockContext(const dim3& gridDim, const dim3& gridId, const dim3& blockDim, void** args, size_t sharedMem);
    std::vector<std::shared_ptr<WarpContext>> GetWarps() const;
};

} // namespace Emulator
