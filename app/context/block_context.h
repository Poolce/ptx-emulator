#pragma once

#include "warp_context.h"

#include <string>
#include <unordered_map>

namespace Emulator
{

class GlobalContext;

class BlockContext : public std::enable_shared_from_this<BlockContext>
{
  private:
    std::weak_ptr<GlobalContext> global_context_;

    std::vector<std::shared_ptr<WarpContext>> warps_;
    std::vector<uint8_t> shared_memory_;

    std::unordered_map<std::string, void*> shared_symbols_;
    size_t shared_offset_ = 0;

  public:
    BlockContext() = default;

    void Init(const std::shared_ptr<GlobalContext>& global_context,
              const dim3& gridDim,
              const dim3& gridId,
              const dim3& blockDim,
              size_t sharedMem);
    std::vector<std::shared_ptr<WarpContext>> GetWarps() const;
    uint64_t GetBasicBlockOffset(const std::string& func_name, const std::string& sym) const;
    std::string GetBasicBlockAt(const std::string& func_name, uint64_t pc) const;
    void* GetParamPtr(const std::string& name) const;
    void RegisterSharedSymbol(const std::string& name, size_t size, size_t align);
    void* GetSharedPtr(const std::string& name) const;
};

} // namespace Emulator
