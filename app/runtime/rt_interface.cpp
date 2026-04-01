#include "rt_interface.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace Emulator
{

void RtInterface::LoadPtx()
{
    const char* env = std::getenv("CUEMU_TARGET_EXEC");
    if (!env)
    {
        throw std::runtime_error("Environment variable CUEMU_TARGET_EXEC not set.");
    }
    auto object = fs::path(env);

    if (!fs::is_regular_file(object))
    {
        throw std::runtime_error("Invalid object path.");
    }

    std::array<char, 512> buffer;
    std::string result;
    auto cmd = std::string("cuobjdump -ptx ") + object.string();

    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe)
    {
        throw std::runtime_error("Decoding object error");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }

    if (result.empty())
    {
        throw std::runtime_error("Ptx section is empty");
    }

    ptx_module_ = Ptx::Module::Make(result);
}

} // namespace Emulator