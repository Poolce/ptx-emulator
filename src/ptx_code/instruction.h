#pragma once

namespace Emulator {
namespace Ptx {

class Instruction {
 private:
  std::vector<Instruction> basic_blocks_;

 public:
  virtual void Execute();

 public:
  Instruction() = delete;
  virtual ~Instruction();
};

}  // namespace Ptx
}  // namespace Emulator