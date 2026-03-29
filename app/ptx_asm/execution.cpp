#include "instructions.h"
#include "utils.h"
#include "warp_context.h"

namespace Emulator
{
namespace Ptx
{

void Instruction::Execute([[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void pragmaInstruction::ExecuteWarp([[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void retInstruction::ExecuteWarp([[maybe_unused]] std::shared_ptr<WarpContext>& wc)
{
    wc->pc = WarpContext::EOC;
}

void braInstruction::ExecuteWarp([[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void paramInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void regInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    wc->thread_regs[lid][reg_] = RegisterContext(count_);
}

void cvtaInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void setpInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void addInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void movInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void andInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void mulInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void stInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void negInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void subInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void labelInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc)
{
}

void ldInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void madInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void cvtInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

} // namespace Ptx
} // namespace Emulator
