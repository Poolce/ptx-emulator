#pragma once

#include "block_context.h"
#include "module.h"

namespace Emulator
{

class GlobalContext : public std::enable_shared_from_this<GlobalContext>
{
  private:
    std::vector<std::shared_ptr<BlockContext>> blocks_;
    std::unordered_map<std::string, Ptx::FunctionParameter> global_parameters_;
    std::shared_ptr<Ptx::Module> ptx_module_;

    void** args_;

  public:
    GlobalContext() = default;
    void Init(const std::shared_ptr<Ptx::Module>& ptx,
              const dim3& gridDim,
              const dim3& blockDim,
              void** args,
              size_t sharedMem);

    std::vector<std::shared_ptr<BlockContext>> GetBlocks() const;
    void SetEntryFunction(const std::string& func_name);

    std::shared_ptr<Ptx::Instruction> GetInstruction(uint64_t pc) const;
    uint64_t GetBasicBlockOffset(const std::string& func_name, const std::string& sym) const;
  
};

} // namespace Emulator
