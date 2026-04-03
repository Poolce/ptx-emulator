#include "global_context.h"

namespace Emulator
{

GlobalContext::GlobalContext(const dim3& gridDim, const dim3& blockDim, void** args, size_t sharedMem)
{
    for (uint32_t gidx = 0; gidx < gridDim.x; ++gidx)
    {
        for (uint32_t gidy = 0; gidy < gridDim.y; ++gidy)
        {
            for (uint32_t gidz = 0; gidz < gridDim.z; ++gidz)
            {
                dim3 gid{gidx, gidy, gidz};
                auto bc = std::make_shared<BlockContext>(gridDim, gid, blockDim, args, sharedMem);
                blocks_.push_back(bc);
            }
        }
    }
}

std::vector<std::shared_ptr<BlockContext>> GlobalContext::GetBlocks() const
{
    return blocks_;
}

} // namespace Emulator
