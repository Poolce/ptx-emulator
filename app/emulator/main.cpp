#include "constant.h"
#include "execution_module.h"
#include "module.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

// std::string readPtx(const std::string& filename)
// {
//     std::ifstream file(filename);
//     if (!file.is_open())
//     {
//         throw std::runtime_error("Failed while opening file: " + filename);
//     }

//     std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

//     if (file.bad())
//     {
//         throw std::runtime_error("Failed while reading file:" + filename);
//     }

//     return content;
// }

} // namespace

int main()
{
    std::string file = "/home/poolce/workplace/ptx-emulator/test/ptx_sources/test";
    auto cmd = std::string("LD_PRELOAD=libemuruntime.so CUEMU_TARGET_EXEC=");
    cmd += file;
    cmd += " ";
    cmd += file;
    return system(cmd.c_str());
}
