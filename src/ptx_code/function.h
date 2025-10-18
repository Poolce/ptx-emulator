#pragma once

#include "basic_block.h"

namespace Emulator {
namespace Ptx {

class Function {
 private:
  std::vector<BasicBlock> basic_blocks_;

 public:
  virtual void Execute();

 public:
  Function() = delete;
  virtual ~Function();
};

}  // namespace Ptx
}  // namespace Emulator
