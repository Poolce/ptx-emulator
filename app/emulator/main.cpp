#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

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

static void PrintHelp(std::string_view prog)
{
    std::cout << "Usage: " << prog << " [OPTIONS] <binary>\n"
              << "\n"
              << "Run a CUDA binary under the PTX emulator.\n"
              << "\n"
              << "Options:\n"
              << "  --collect-profiling          Enable profiling metric collection\n"
              << "  --profiling-output <path>    Output file for profiling data (default: profiling.txt)\n"
              << "  -h, --help                   Show this help message and exit\n"
              << "\n"
              << "Examples:\n"
              << "  " << prog << " ./my_cuda_app\n"
              << "  " << prog << " --collect-profiling --profiling-output prof.log ./my_cuda_app\n";
}

int main(int argc, char* argv[])
{
    bool collect_profiling = false;
    std::string profiling_output;
    std::string binary;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            PrintHelp(argv[0]);
            return 0;
        }
        if (arg == "--collect-profiling")
        {
            collect_profiling = true;
        }
        else if (arg == "--profiling-output")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "error: --profiling-output requires an argument\n";
                return 1;
            }
            profiling_output = argv[++i];
        }
        else if (arg.starts_with('-'))
        {
            std::cerr << "error: unknown option '" << arg << "'\n"
                      << "Run '" << argv[0] << " --help' for usage.\n";
            return 1;
        }
        else
        {
            if (!binary.empty())
            {
                std::cerr << "error: unexpected argument '" << arg << "'" << " (binary already set to '" << binary
                          << "')\n";
                return 1;
            }
            binary = arg;
        }
    }

    if (binary.empty())
    {
        std::cerr << "error: missing required argument <binary>\n\n";
        PrintHelp(argv[0]);
        return 1;
    }

    std::string cmd;
    cmd += "PATH=" + GetExecutableDir().string() + ":$PATH ";
    cmd += "LD_LIBRARY_PATH=" + GetLibDir().string() + ":$LD_LIBRARY_PATH ";
    cmd += "LD_PRELOAD=" + std::string(RuntimeLibName) + " ";
    cmd += "CUEMU_TARGET_EXEC=" + binary + " ";
    if (collect_profiling)
    {
        cmd += "CUEMU_COLLECT_PROFILING=1 ";
        if (!profiling_output.empty())
        {
            cmd += "CUEMU_PROFILING_OUTPUT=" + profiling_output + " ";
        }
    }
    cmd += binary;

    return system(cmd.c_str());
}
