#ifndef _FEEDS_COMMAND_HANDLER_HPP_
#define _FEEDS_COMMAND_HANDLER_HPP_

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <RpcFactory.hpp>
#include <StdFileSystem.hpp>

#include <ela_carrier.h>
extern "C" {
#include <obj.h>
#include <rpc.h>
#include <msgq.h>
}

struct ElaSession;
struct ElaStreamCallbacks;

namespace trinity {

class ThreadPool;

class CommandHandler {
public:
    /*** type define ***/
    class Listener {
    public:
        enum Accessible {
            Anyone,
            Member,
            Owner,
        };

    protected:
        struct NormalHandler {
            std::function<int(std::shared_ptr<Req>, std::shared_ptr<Resp>&)> callback;
            Accessible accessible;
        };
        struct AdvancedHandler {
            std::function<int(std::shared_ptr<Rpc::Request>, std::vector<std::shared_ptr<Rpc::Response>>&)> callback;
            Accessible accessible;
        };

        static const std::filesystem::path& GetDataDir();

        explicit Listener() = default;
        virtual ~Listener() = default;

        void setHandleMap(const std::map<const char*, NormalHandler>& normalHandlerMap,
                          const std::map<const char*, AdvancedHandler>& advancedHandlerMap);

        virtual int checkAccessible(Accessible accessible, const std::string& accessToken);
        virtual int onDispose(const std::string& from,
                              std::shared_ptr<Req> req,
                              std::shared_ptr<Resp>& resp);
        virtual int onDispose(std::shared_ptr<Rpc::Request> request,
                              std::vector<std::shared_ptr<Rpc::Response>>& responseArray);
    private:
        static int SetDataDir(const std::filesystem::path& dataDir);
        static std::filesystem::path DataDir;

        int isOwner(const std::string& accessToken);
        int isMember(const std::string& accessToken);
        int getUserInfo(const std::string& accessToken, std::shared_ptr<UserInfo>& userInfo);

        std::map<const char *, NormalHandler> normalHandlerMap;
        std::map<const char *, AdvancedHandler> advancedHandlerMap;

        friend CommandHandler;
    };

    /*** static function and variable ***/
    static std::shared_ptr<CommandHandler> GetInstance();
    static void PrintElaCarrierError(const std::string &errReason);
    static constexpr const char* CacheDirName = "cache";

    /*** class function and variable ***/
    int config(const std::filesystem::path &execPath,
               const std::filesystem::path &dataDir,
               std::weak_ptr<ElaCarrier> carrier);
    void cleanup();

    std::weak_ptr<ElaCarrier> getCarrierHandler();

    int received(const std::string& from, const std::vector<uint8_t>& data);
    int send(const std::string &to, const std::vector<uint8_t>& data,
             ElaFriendMessageReceiptCallback* receiptCallback = nullptr, void* receiptContext = nullptr);

    int unpackRequest(const std::vector<uint8_t>& data,
                      std::shared_ptr<Req>& req) const;
    int packResponse(const std::shared_ptr<Req>& req,
                     const std::shared_ptr<Resp>& resp,
                     int errCode,
                     std::vector<uint8_t>& data) const;

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/
    static std::shared_ptr<CommandHandler> CmdHandlerInstance;

    /*** class function and variable ***/
    explicit CommandHandler() = default;
    virtual ~CommandHandler() = default;
    int process(const std::string& from, const std::vector<uint8_t>& data);
    int processAdvance(const std::string& from, const std::vector<uint8_t>& data);

    std::shared_ptr<ThreadPool> threadPool;
    std::weak_ptr<ElaCarrier> carrierHandler;
    std::vector<std::shared_ptr<Listener>> cmdListener;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _FEEDS_COMMAND_HANDLER_HPP_ */


