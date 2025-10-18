#pragma once

namespace Emulator {
namespace Context {

class GlobalContext {
 private:
  std::vector<BlockContext> block_contecsts;

  std::shared_ptr<ptx::Module> ptx_module_;

  std::vector<uint8_t> global_memory;

 public:
  GlobalContext() = default;
  ~GlobalContext() = default;

  template <typename T>
  T GlobalLoad(uint64_t address);

  template <typename T>
  void GlobalStore(uint64_t address, T value);
};

}  // namespace Context
}  // namespace Emulator
