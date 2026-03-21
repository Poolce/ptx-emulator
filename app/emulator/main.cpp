#include "execution_module.h"
#include "module.h"

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
    auto ptx_text = readPtx("/home/poolce/workplace/ptx-emulator/test/ptx_sources/vadd.ptx");
    auto ptx_module = Emulator::Ptx::Module::Make(ptx_text);
    auto entry = ptx_module->GetEntryFunc();
    entry->Dump();
    auto exec_ = Emulator::ExecutionModule(ptx_module);
    return 0;
}
