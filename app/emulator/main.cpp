#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <binary>\n";
        return 1;
    }
    std::string file = argv[1];
    auto cmd = std::string("LD_PRELOAD=libemuruntime.so CUEMU_TARGET_EXEC=");
    cmd += file;
    cmd += " ";
    cmd += file;
    return system(cmd.c_str());
}
