#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace Emulator {
namespace Ptx {
class Module;
} // namespace Ptx

namespace Context {

class GlobalContext;
class WarpContext;

class BlockContext
{
  private:
    std::shared_ptr<GlobalContext> global_context_;
    std::vector<WarpContext> warp_contexts_;

    std::shared_ptr<Ptx::Module> ptx_module_;

    std::vector<std::uint8_t> shared_memory_;

  public:
    BlockContext() = default;
    ~BlockContext() = default;

    template <typename T>
    T SharedLoad(std::uint64_t address);

    template <typename T>
    void SharedStore(std::uint64_t address, T value);

    template <typename T>
    T GlobalLoad(std::uint64_t address);

    template <typename T>
    void GlobalStore(std::uint64_t address, T value);
};

} // namespace Context
} // namespace Emulator
