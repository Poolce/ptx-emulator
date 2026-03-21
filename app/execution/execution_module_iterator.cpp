#include "execution_module.h"

namespace Emulator
{

using const_iter = ExecutionModule::const_iterator;

const_iter::const_iterator() : module_(nullptr), is_end_(true) {}

const_iter::const_iterator(ExecutionModule* module, bool is_end) : module_(module), is_end_(is_end)
{
    if (!is_end_ && module_)
    {
        module_->Reset();
        current_instruction_ = module_->NextInstruction();
        if (!current_instruction_)
        {
            is_end_ = true;
        }
    }
}

const_iter::reference const_iter::operator*() const
{
    return *current_instruction_;
}
const_iter::pointer const_iter::operator->() const
{
    return current_instruction_.get();
}

const_iter& const_iter::operator++()
{
    if (!is_end_ && module_)
    {
        current_instruction_ = module_->NextInstruction();
        if (!current_instruction_)
        {
            is_end_ = true;
        }
    }
    return *this;
}

const_iter const_iter::operator++(int)
{
    const_iter tmp = *this;
    ++(*this);
    return tmp;
}

bool const_iter::operator==(const const_iter& other) const
{
    if (is_end_ && other.is_end_)
        return true;
    if (is_end_ || other.is_end_)
        return false;
    return current_instruction_ == other.current_instruction_;
}

bool const_iter::operator!=(const const_iter& other) const
{
    return !(*this == other);
}

} // namespace Emulator
