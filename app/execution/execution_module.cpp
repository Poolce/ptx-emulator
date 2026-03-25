#include "execution_module.h"

namespace Emulator
{

void ExecutionModule::Reset()
{
    
}

std::shared_ptr<Ptx::Instruction> ExecutionModule::NextInstruction()
{
    return {};
}

ExecutionModule::ExecutionModule(const std::shared_ptr<Ptx::Module>& module) : module_(module) {}

ExecutionModule::const_iterator ExecutionModule::begin()
{
    return const_iterator(this, false);
}

ExecutionModule::const_iterator ExecutionModule::end()
{
    return const_iterator(this, true);
}

} // namespace Emulator
