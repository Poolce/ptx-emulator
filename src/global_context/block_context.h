#pragma once

namespace Emulator {
namespace Context {

class BlockContext {
 private:
  std::vector<WarpContext> warp_contecsts_;
  std::vector<uint8_t> shared_memory_;

  std::shared_ptr<GlobalContext> global_context_;

  std::shared_ptr<ptx::Module> ptx_module_;

 public:
  BlockContext() = default;
  ~BlockContext() = default;

  template <typename T>
  T SharedLoad(uint64_t address);

  template <typename T>
  void SharedStore(uint64_t address, T value);
};

}  // namespace Context
}  // namespace Emulator
