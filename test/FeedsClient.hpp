#ifndef _FEEDS_CLIENT_HPP_
#define _FEEDS_CLIENT_HPP_

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
//#include <CompatibleFileSystem.hpp>

namespace elastos {

class FeedsClient {
public:
    /*** type define ***/

    /*** static function and variable ***/
    static std::shared_ptr<FeedsClient> GetInstance();

    /*** class function and variable ***/
    void init();
    void loop(const std::string& fifoFilePath);

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/
    struct CommandInfo {
        using Processor = int(const std::vector<std::string>&, std::string&);

        char cmd;
        std::string longCmd;
        std::function<Processor> handler;
        std::string usage;
    };

    /*** static function and variable ***/
    static std::shared_ptr<FeedsClient> FeedsClientInstance;

    /*** class function and variable ***/
    explicit FeedsClient() noexcept = default;
    virtual ~FeedsClient() noexcept = default;

    void parseCmd(const std::string& cmdLine);
    int doCmd(const std::string& cmdLine, std::string& errMsg);
    int onUnimplemention(const std::vector<std::string>& args,
                         std::string& errMsg);
    int onQuit(const std::vector<std::string>& args,
               std::string& errMsg);
    int onHelp(const std::vector<std::string>& args,
               std::string& errMsg);

    bool quitFlag;
    std::vector<CommandInfo> cmdInfoList;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace elastos

#endif /* _FEEDS_CLIENT_HPP_ */

