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
    bool collect_profiling = false;
    std::string profiling_output;
    int binary_arg = -1;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "--collect-profiling")
        {
            collect_profiling = true;
        }
        else if (arg == "--profiling-output" && i + 1 < argc)
        {
            profiling_output = argv[++i];
        }
        else
        {
            binary_arg = i;
        }
    }

    if (binary_arg < 0)
    {
        std::cerr << "Usage: " << argv[0]
                  << " [--collect-profiling] [--profiling-output <path>] <binary>\n";
        return 1;
    }

    const std::string file = argv[binary_arg];

    std::string cmd;
    cmd += "PATH=" + GetExecutableDir().string() + ":$PATH" + " ";
    cmd += "LD_LIBRARY_PATH=" + GetLibDir().string() + ":$LD_LIBRARY_PATH" + " ";
    cmd += "LD_PRELOAD=" + std::string(RuntimeLibName) + " ";
    cmd += "CUEMU_TARGET_EXEC=" + file + " ";
    if (collect_profiling)
    {
        cmd += "CUEMU_COLLECT_PROFILING=1 ";
        if (!profiling_output.empty())
        {
            cmd += "CUEMU_PROFILING_OUTPUT=" + profiling_output + " ";
        }
    }
    cmd += file;

    return system(cmd.c_str());
}
