#pragma once

#include "module.h"

#include <iterator>

namespace Emulator
{

class ExecutionModule
{
  private:
    std::shared_ptr<Ptx::Module> module_;

    std::shared_ptr<Ptx::Function> current_function_;
    std::shared_ptr<Ptx::BasicBlock> current_basic_block_;

    std::shared_ptr<Ptx::Instruction> NextInstruction();
    void Reset();

  public:
    ExecutionModule(const std::shared_ptr<Ptx::Module>& module);

    class const_iterator
    {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Ptx::Instruction;
        using difference_type = std::ptrdiff_t;
        using pointer = const Ptx::Instruction*;
        using reference = const Ptx::Instruction&;

        const_iterator();
        const_iterator(ExecutionModule* module, bool is_end);

        reference operator*() const;
        pointer operator->() const;
        const_iterator& operator++();
        const_iterator operator++(int);
        bool operator==(const const_iterator& other) const;
        bool operator!=(const const_iterator& other) const;

      private:
        ExecutionModule* module_;
        std::shared_ptr<Ptx::Instruction> current_instruction_;
        bool is_end_;
    };

    const_iterator begin();
    const_iterator end();
};

} // namespace Emulator
