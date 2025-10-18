#pragma once

namespace Emulator {
namespace Ptx {

class BasicBlock {
 private:
  std::vector<Instruction> instructions_;

 public:
  virtual void Execute();

 public:
  BasicBlock() = delete;
  virtual ~BasicBlock();
};

}  // namespace Ptx
}  // namespace Emulator