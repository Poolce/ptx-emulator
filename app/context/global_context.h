#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace Emulator
{
namespace Ptx
{
class Module;
} // namespace Ptx

namespace Context
{

class BlockContext;

class GlobalContext
{
  private:
    std::vector<BlockContext> block_contexts_;

    std::shared_ptr<Ptx::Module> ptx_module_;

    std::vector<std::uint8_t> global_memory_;

  public:
    GlobalContext() = default;
    ~GlobalContext() = default;

    template <typename T>
    T GlobalLoad(std::uint64_t address);

    template <typename T>
    void GlobalStore(std::uint64_t address, T value);
};

} // namespace Context
} // namespace Emulator
