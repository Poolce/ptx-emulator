#include "execution_module.h"

namespace Emulator
{

ExecutionModule::ExecutionModule(const std::shared_ptr<Ptx::Module>& module) : module_(module) {}

std::shared_ptr<Ptx::Instruction> ExecutionModule::GetInstruction(std::shared_ptr<WarpContext>& wc) const
{
    if (wc->pc == WarpContext::EOC)
    {
        if (wc->execution_stack.empty())
        {
            return nullptr;
        }
        else
        {
            auto [pc, mask] = wc->execution_stack.top();
            wc->pc = pc;
            wc->execution_mask = mask;
            wc->execution_stack.pop();
            return module_->GetInstruction(wc->pc);
        }
    }
    return module_->GetInstruction(wc->pc);
}

void ExecutionModule::SetEntryFunction(std::shared_ptr<WarpContext>& wc, const std::string& func_name) const
{
    auto func = module_->GetEntryFunction(func_name);
    wc->pc = func->getOffset();
    wc->global_parameters = func->getParameters();
}

} // namespace Emulator
