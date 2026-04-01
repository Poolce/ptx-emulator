#pragma once

#include "ptx_types.h"

#include <cstdint>
#include <iterator>
#include <stack>
#include <unordered_map>
#include <vector>

namespace Emulator
{

using FuncDescriptor = uint64_t;

struct FunctionContext
{
    std::string name;
    std::vector<uint64_t> FunctionParams;
};

class GlobalContext
{
  private:
    std::unordered_map<FuncDescriptor, FunctionContext> func_context;

  public:
    int RegisterFunction(const std::string func_name, uint64_t descr, void* params);
    // int SetDimention(dim3 grid, dim3 block);

    GlobalContext();
};

} // namespace Emulator
