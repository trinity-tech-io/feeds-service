#include <iostream>
#include <string>
#include <unistd.h>
#include <ErrCode.hpp>
#include <Log.hpp>
#include <Platform.hpp>
#include <StdFileSystem.hpp>
using trinity::ErrCode;
using trinity::Log;

#include <crystal/vlog.h>

static void print_backtrace(int sig)
{
    std::cerr << "Error: signal " << sig << std::endl;

    std::string backtrace = trinity::Platform::GetBacktrace();
    std::cerr << backtrace << std::endl;

    exit(sig);
}

extern "C"
const char *err_strerror(int rc)
{
    return "unknown";
}

int main(int argc, char const *argv[])
{
    signal(SIGSEGV, print_backtrace);
    signal(SIGABRT, print_backtrace);

    std::filesystem::path runtimePath = argv[1];
    std::string verCode = argv[2];

    Log::I(Log::Tag::AU, "Runtime path is %s", runtimePath.c_str());
    Log::I(Log::Tag::AU, "Switch current runtime to version %s", verCode.c_str());

    auto parentPid = getppid();
    Log::I(Log::Tag::AU, "Killing parent process[%d]", parentPid);
    auto cmdline = "kill -2 " + std::to_string(parentPid);
    int ret = std::system(cmdline.c_str());
    CHECK_ASSERT(ret == 0, ErrCode::ExecSystemCommendFailed);

    while(true) {
        sleep(1);
    }

    return 0;
}