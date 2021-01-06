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

extern "C"
const char *err_strerror(int rc)
{
    return "unknown";
}

namespace trinity {
static void PrintBacktrace(int sig)
{
    std::cerr << "Error: signal " << sig << std::endl;

    std::string backtrace = Platform::GetBacktrace();
    std::cerr << backtrace << std::endl;

    exit(sig);
}

static bool FeedsdProcessExists() {
    std::string cmd = "pgrep feedsd |grep -v " + std::to_string(getpid());
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

static bool FeedsdProcessStatus(bool isExisting) {
    auto status = !isExisting;
    for(auto retry = 0; retry < 3; retry++) {
        status = FeedsdProcessExists();
        if(status == isExisting) {
            break;
        }
        sleep(1);
    }

    return (status == isExisting);
}

static int MakeSymLink(const std::string& target, const std::filesystem::path& link)
{
    std::error_code ec;

    Log::I(Log::Tag::AU, "Removing old symlink: %s", link.c_str());
    if (std::filesystem::is_symlink(link) == true
    || std::filesystem::exists(link) == true) {
        std::filesystem::remove(link, ec);
        if (ec.value() != 0) {
            Log::E(Log::Tag::AU, "Failed to remove %s. err: %s(%d)",
                                link.c_str(),
                                ec.message().c_str(), ec.value());
        }
        CHECK_ASSERT(ec.value() == 0, ErrCode::AutoUpdateRemoveLinkFailed);
    }

    Log::I(Log::Tag::AU, "Creating new symlink to: %s", target.c_str());
    create_directory_symlink(target, link, ec) ;
    if (ec.value() != 0) {
        Log::E(Log::Tag::AU, "Failed to create link %s ==> %s. err: %s(%d)",
                             link.c_str(), target.c_str(),
                             ec.message().c_str(), ec.value());
    }
    CHECK_ASSERT(ec.value() == 0, ErrCode::AutoUpdateMakeLinkFailed);

    return 0;
}

static int KillFeedsdProcess(const std::string& feedsdPid)
{
    Log::I(Log::Tag::AU, "Killing parent process[%s]", feedsdPid.c_str());
    int ppid = std::stoi(feedsdPid);
    kill(ppid, SIGINT);

    bool exited = FeedsdProcessStatus(false);
    Log::I(Log::Tag::AU, "Parent process[%s] exit %s", feedsdPid.c_str(), exited ? "success" : "failed");
    CHECK_ASSERT(exited == true, ErrCode::AutoUpdateKillRuntimeFailed);

    return 0;
}

static int LaunchFeedsdProcess(const std::string& cmdline)
{
    auto launchCmd = cmdline + "&";
    Log::I(Log::Tag::AU, "Launching feeds service with command: %s.", launchCmd.c_str());
    int ret = std::system(launchCmd.c_str());
    CHECK_ASSERT(ret == 0, ErrCode::ExecSystemCommendFailed);

    bool exists = FeedsdProcessStatus(true);
    Log::I(Log::Tag::AU, "Feedsd process start %s with command: %s", exists ? "success" : "failed", launchCmd.c_str());
    CHECK_ASSERT(exists == true, ErrCode::AutoUpdateStartRuntimeFailed);

    return 0;
}

static int SwitchFeedsdTo(const std::string& verName,
                          const std::filesystem::path& currentPath,
                          const std::string& feedsdPid,
                          const std::string& launchCmd)
{
    int ret;
    
    Log::I(Log::Tag::AU, "Switching symlink %s ==> %s",
                         currentPath.c_str(), verName.c_str());

    ret = MakeSymLink(verName, currentPath);
    CHECK_ERROR(ret);

    ret = KillFeedsdProcess(feedsdPid);
    CHECK_ERROR(ret);

    ret = LaunchFeedsdProcess(launchCmd);
    CHECK_ERROR(ret);

    return 0;
}

}

int main(int argc, char const *argv[])
{
    signal(SIGSEGV, trinity::PrintBacktrace);
    signal(SIGABRT, trinity::PrintBacktrace);

    CHECK_ASSERT(argc == 6, ErrCode::InvalidArgument);

    std::filesystem::path runtimeDir = argv[1];
    std::string verName = argv[2];
    std::string parentPid = argv[3];
    std::string launchCmd = argv[4];
    std::string logFile = argv[5];

    vlog_init(VLOG_DEBUG, logFile.c_str(), nullptr); 
    Log::I(Log::Tag::AU, "Log file path is %s", logFile.c_str());
    Log::I(Log::Tag::AU, "Runtime path is %s", runtimeDir.c_str());
    Log::I(Log::Tag::AU, "New runtime version is %s", verName.c_str());

    auto currentPath = runtimeDir / "current";
    std::error_code ec;
    auto oldSymlink = std::filesystem::read_symlink(currentPath, ec);
    if (ec.value() != 0) {
        Log::E(Log::Tag::AU, "Failed to backup old symlink %s. err: %s(%d)",
                             currentPath.c_str(),
                             ec.message().c_str(), ec.value());
    }
    CHECK_ASSERT(ec.value() == 0, ErrCode::AutoUpdateReadLinkFailed);
    Log::I(Log::Tag::AU, "Backup old symlink: %s", oldSymlink.c_str());

    int ret = trinity::SwitchFeedsdTo(verName, currentPath, parentPid, launchCmd);
    if(ret >= 0) {
        return 0;
    }
    auto failedReason = ret;

    // Failed to update to new version, recovery to old version.
    Log::W(Log::Tag::AU, "Failed to switch feedsd to: %s, tryto recovery old version: %s.",
                         verName.c_str(), oldSymlink.c_str());
    ret = trinity::SwitchFeedsdTo(oldSymlink, currentPath, parentPid, launchCmd);
    CHECK_ERROR(ret);

    return failedReason;
}