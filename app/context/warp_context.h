#pragma once

namespace Emulator
{
namespace Context
{

class BlockContext
{
  private:
    std::shared_ptr<BlockContext> block_context_;
    std::vector<ThreadContext> thread_contecsts_;

    std::shared_ptr<ptx::Module> ptx_module_;

  public:
    BlockContext() = default;
    ~BlockContext() = default;

    template <typename T>
    T SharedLoad(uint64_t address);

    template <typename T>
    void SharedStore(uint64_t address, T value);

    template <typename T>
    T GlobalLoad(uint64_t address);

    template <typename T>
    void GlobalStore(uint64_t address, T value);
};

} // namespace Context
} // namespace Emulator
