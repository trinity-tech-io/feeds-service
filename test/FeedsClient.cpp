#include "FeedsClient.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <signal.h>
#include <sstream>
#include <Log.hpp>
#include <Platform.hpp>

void signalHandler(int sig) {
    std::cerr << "Error: signal " << sig << std::endl;

    std::string backtrace = elastos::Platform::GetBacktrace();
    std::cerr << backtrace << std::endl;

    exit(sig);
}

int main(int argc, char **argv)
{
    signal(SIGSEGV, signalHandler);
    signal(SIGABRT, signalHandler);

    Log::I(Log::TAG, "Feeds client start.");
    Log::I(Log::TAG, "%s", (argc > 1 ? argv[1]:""));

    const char* fifoFilePath = (argc > 1 ? argv[1] : "");

    auto client = elastos::FeedsClient::GetInstance();
    client->init();
    client->loop(fifoFilePath);

    return 0;
}

namespace elastos {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */
std::shared_ptr<FeedsClient> FeedsClient::FeedsClientInstance;

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */
std::shared_ptr<FeedsClient> FeedsClient::GetInstance()
{
    // ignore thread-safty, no needed

    if(FeedsClientInstance != nullptr) {
        return FeedsClientInstance;
    }

    struct Impl: FeedsClient {
    };
    FeedsClientInstance = std::make_shared<Impl>();

    return FeedsClientInstance;
}

void FeedsClient::init()
{
    using namespace std::placeholders;

    this->cmdInfoList = std::move(std::vector<CommandInfo> {
        {
            'q', "quit", std::bind(&FeedsClient::onQuit, this, _1, _2),
            "[q]Quit."
        }, {
            'h', "help", std::bind(&FeedsClient::onHelp, this, _1, _2),
            "[h]Print help usages. "
        },
    });
}

void FeedsClient::loop(const std::string& fifoFilePath)
{
    while (true) {
        std::string cmdLine;
        std::getline(std::cin, cmdLine);

        parseCmd(cmdLine);
    }
}

int FeedsClient::doCmd(const std::string& cmdLine,
                       std::string& errMsg)
{
    auto wsfront=std::find_if_not(cmdLine.begin(), cmdLine.end(),
                                 [](int c){return std::isspace(c);});
    auto wsback=std::find_if_not(cmdLine.rbegin(), cmdLine.rend(),
                                 [](int c){return std::isspace(c);}).base();
    auto trimCmdLine = (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));

    std::istringstream iss(trimCmdLine);
    std::vector<std::string> args {std::istream_iterator<std::string>{iss},
                                   std::istream_iterator<std::string>{}};
    if (args.size() <= 0) {
        return 0;
    }
    for(auto& it: args) {
        auto wsfront=std::find_if_not(it.begin(), it.end(),
                                      [](int c){return c == '"';});
        auto wsback=std::find_if_not(it.rbegin(), it.rend(),
                                     [](int c){return c == '"';}).base();
        it = (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
    }
    const auto& cmd = args[0];

    for(const auto& cmdInfo : cmdInfoList) {
        if(cmdInfo.cmd == ' '
        || cmdInfo.handler == nullptr) {
            continue;
        }
        if(cmd.compare(0, 1, &cmdInfo.cmd) != 0
        && cmd != cmdInfo.longCmd) {
            continue;
        }

        int ret = cmdInfo.handler(args, errMsg);
        return ret;
    }

    errMsg = "Unknown command: " + cmd;
    return -10000;
}

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
void FeedsClient::parseCmd(const std::string& cmdLine)
{
    if (cmdLine.empty() == true) {
        std::cout << "# ";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return;
    }
    Log::I(Log::TAG, "==> Received Command: %s", cmdLine.c_str());

    std::string errMsg;
    int ret = doCmd(cmdLine, errMsg);
    if (ret < 0) {
        Log::E(Log::TAG, "ErrCode(%d): %s", ret, errMsg.c_str());
    } else {
        Log::I(Log::TAG, "Success to exec: %s", cmdLine.c_str());
    }
    std::cout << "# ";
}

int FeedsClient::onUnimplemention(const std::vector<std::string>& args,
                                  std::string& errMsg)
{
    errMsg = "Unimplemention Command!!!";
    return -1;
}

int FeedsClient::onQuit(const std::vector<std::string>& args,
                        std::string& errMsg)
{
    quitFlag = true;
    return 0;
}

int FeedsClient::onHelp(const std::vector<std::string>& args,
                        std::string& errMsg)
{
    std::cout << "Usage:" << std::endl;
    for(const auto& cmdInfo : cmdInfoList) {
        if(cmdInfo.cmd == '-') {
            std::cout << cmdInfo.usage << std::endl;
        } else {
            std::cout << "  " << cmdInfo.cmd << " | " << cmdInfo.longCmd << ": " << cmdInfo.usage << std::endl;
        }
    }
    std::cout << std::endl;

    return 0;
}

} // namespace elastos