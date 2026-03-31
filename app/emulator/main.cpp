#include "constant.h"
#include "execution_module.h"
#include "module.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

std::string readPtx(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed while opening file: " + filename);
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    if (file.bad())
    {
        throw std::runtime_error("Failed while reading file:" + filename);
    }

    return content;
}

} // namespace

int main()
{
    auto start = std::chrono::high_resolution_clock::now();

    auto ptx_text = readPtx("/home/poolce/workplace/ptx-emulator/test/ptx_sources/vadd.ptx");
    auto ptx_module = Emulator::Ptx::Module::Make(ptx_text);
    auto exec_module = Emulator::ExecutionModule(ptx_module);
    auto context = std::make_shared<Emulator::WarpContext>();

    exec_module.SetEntryFunction(context, "_Z9vectorAddPKmS0_Pmi");

    auto spr_context = std::vector<Emulator::SprContext>(Emulator::WARP_SIZE);
    for (int i = 0; i < Emulator::WARP_SIZE; i++)
    {
        spr_context[i] = {{Emulator::Ptx::sprType::Tid, i},
                          {Emulator::Ptx::sprType::Ntid, 32},
                          {Emulator::Ptx::sprType::Ctaid, 0}};
    }

    auto instr = exec_module.GetInstruction(context);
    while (instr)
    {
        instr->Execute(context);
        instr = exec_module.GetInstruction(context);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    auto time_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Time: " << time_ms << "\n";
    return 0;
}
