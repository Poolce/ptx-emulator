#include "execution_functions.h"
#include "instructions.h"
#include "utils.h"
#include "warp_context.h"
using namespace __ExecutionFunctions;

namespace Emulator
{
namespace Ptx
{
void Instruction::Execute([[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

// Branch Instructions

void retInstruction::ExecuteBranch([[maybe_unused]] std::shared_ptr<WarpContext>& wc)
{
    wc->pc = WarpContext::EOC;
}

void braInstruction::ExecuteBranch([[maybe_unused]] std::shared_ptr<WarpContext>& wc)
{
    wc->pc++;
}

// Meta Instructions

void pragmaInstruction::ExecuteWarp([[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void regInstruction::ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc)
{
    wc->thread_regs[lid][reg_] = RegisterContext(count_);
}

// Load/Store Instructions

void stInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void ldInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

// Calculation Instructions

void cvtaInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void setpInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void addInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc)
{
    uint64_t a_op = wc->thread_regs[lid][src1_.type][src1_.reg_id];
    uint64_t b_op = wc->thread_regs[lid][src2_.type][src2_.reg_id];
    wc->thread_regs[lid][dst_.type][dst_.reg_id] = a_op + b_op;
}

void andInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void mulInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void negInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void subInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

void labelInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc)
{
}

void madInstruction::ExecuteThread([[maybe_unused]] uint32_t lid, [[maybe_unused]] std::shared_ptr<WarpContext>& wc) {}

} // namespace Ptx
} // namespace Emulator
