#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

constexpr const char* RuntimeLibName = "libemuruntime.so";

static fs::path GetExecutableDir()
{
    std::error_code ec;
    auto exe_path = fs::read_symlink("/proc/self/exe", ec);
    if (!ec)
    {
        auto dir = exe_path.parent_path();
        if (!dir.empty())
        {
            return dir;
        }
    }
    return ".";
}

static fs::path GetLibDir()
{
    fs::path lib_path = GetExecutableDir() / ".." / "lib";
    return lib_path.lexically_normal();
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <binary>\n";
        return 1;
    }
    const std::string file = argv[1];

    std::string cmd;
    cmd += "PATH=" + GetExecutableDir().string() + ":$PATH" + " ";
    cmd += "LD_LIBRARY_PATH=" + GetLibDir().string() + ":$LD_LIBRARY_PATH" + " ";
    cmd += "LD_PRELOAD=" + std::string(RuntimeLibName) + " ";
    cmd += "CUEMU_TARGET_EXEC=" + file + " ";
    cmd += file;

    return system(cmd.c_str());
}
