#include "warp_context.h"

#include "constant.h"

namespace Emulator
{

WarpContext::WarpContext()
{
    thread_regs = std::vector<ThreadContext>(WARP_SIZE);
    spr_regs = std::vector<SprContext>(WARP_SIZE);
}

} // namespace Emulator