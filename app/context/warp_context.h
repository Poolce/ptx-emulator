#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace Emulator {
namespace Ptx {
class Module;
} // namespace Ptx

namespace Context {

class BlockContext;
class ThreadContext;

class WarpContext
{
  private:
    std::shared_ptr<BlockContext> block_context_;
    std::vector<ThreadContext> thread_contexts_;

    std::shared_ptr<Ptx::Module> ptx_module_;

  public:
    WarpContext() = default;
    ~WarpContext() = default;

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
