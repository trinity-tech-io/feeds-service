#include <array>
#include <iostream>
#include <signal.h>
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

bool feedsdProcessExists() {
    std::string cmd = "pgrep 'feedsd '";
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return (result.empty() == false);
}

int main(int argc, char const *argv[])
{
    signal(SIGSEGV, print_backtrace);
    signal(SIGABRT, print_backtrace);

    CHECK_ASSERT(argc == 5, ErrCode::InvalidArgument);

    std::filesystem::path runtimeDir = argv[1];
    std::string verCode = argv[2];
    std::string parentPid = argv[3];
    std::string launchCmd = argv[4];

    Log::I(Log::Tag::AU, "Runtime path is %s", runtimeDir.c_str());
    Log::I(Log::Tag::AU, "New runtime version is %s", verCode.c_str());

    Log::I(Log::Tag::AU, "Killing parent process[%s]", parentPid.c_str());
    int ppid = std::stoi(parentPid);
    kill(ppid, SIGKILL);
    bool exited = false;
    for(auto retry = 0; retry < 3; retry++) {
        exited = (feedsdProcessExists() == false);
        if(exited == true) {
            break;
        }
        sleep(1);
    }
    Log::I(Log::Tag::AU, "Parent process[%s] exit %s", parentPid.c_str(), exited ? "success" : "failed");
    CHECK_ASSERT(exited == true, ErrCode::AutoUpdateKillRuneimeFailed);

    auto currentPath = runtimeDir / "current";
    std::error_code ec;
    Log::I(Log::Tag::AU, "Switch symlink %s ==> %s",
                         currentPath.c_str(), verCode.c_str());

    auto oldSymlink = std::filesystem::read_symlink(currentPath, ec);
    if (ec.value() != 0) {
        Log::E(Log::Tag::AU, "Failed to read symlink %s. err: %s(%d)",
                             currentPath.c_str(),
                             ec.message().c_str(), ec.value());
    }
    CHECK_ASSERT(ec.value() == 0, ErrCode::AutoUpdateReadLinkFailed);
    Log::D(Log::Tag::AU, "Backup old symlink: %s", oldSymlink.c_str());

    Log::D(Log::Tag::AU, "Removing old symlink: %s", currentPath.c_str());
    std::filesystem::remove(currentPath, ec);
    if (ec.value() != 0) {
        Log::E(Log::Tag::AU, "Failed to remove %s. err: %s(%d)",
                             currentPath.c_str(),
                             ec.message().c_str(), ec.value());
    }
    CHECK_ASSERT(ec.value() == 0, ErrCode::AutoUpdateRemoveLinkFailed);

    Log::D(Log::Tag::AU, "Creating new symlink to: %s", verCode.c_str());
    create_directory_symlink(verCode, currentPath, ec) ;
    if (ec.value() != 0) {
        Log::E(Log::Tag::AU, "Failed to create link %s ==> %s. err: %s(%d)",
                             currentPath.c_str(), verCode.c_str(),
                             ec.message().c_str(), ec.value());
    }
    CHECK_ASSERT(ec.value() == 0, ErrCode::AutoUpdateMakeLinkFailed);

    launchCmd += "&";
    Log::I(Log::Tag::AU, "Launching feeds service with command: %s.", launchCmd.c_str());
    int ret = std::system(launchCmd.c_str());
    CHECK_ASSERT(ret == 0, ErrCode::ExecSystemCommendFailed);
    auto exists = false;
    for(auto retry = 0; retry < 3; retry++) {
        exists = feedsdProcessExists();
        if(exists == true) {
            break;
        }
        sleep(1);
    }
    Log::I(Log::Tag::AU, "Feedsd process start %s with command: %s", exists ? "success" : "failed", launchCmd.c_str());
    // CHECK_ASSERT(exists == true, ErrCode::AutoUpdateStartRuneimeFailed);

    while(1) {
        sleep(1);
    }

    return 0;
}