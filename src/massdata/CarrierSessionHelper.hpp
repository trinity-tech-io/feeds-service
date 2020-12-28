#ifndef _CARRIER_SESSION_HPP_
#define _CARRIER_SESSION_HPP_

#include <future> 
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

struct Carrier;
struct CarrierSession;
struct CarrierStreamCallbacks;

namespace trinity {

class ThreadPool;

class CarrierSessionHelper : public std::enable_shared_from_this<CarrierSessionHelper> {
public:
    /*** type define ***/
    class Factory {
    public:
        using OnRequest = void(std::weak_ptr<Carrier> carrier,
                             const std::string& from, const std::string& sdp);

        static int Init(std::weak_ptr<Carrier> carrier, const std::function<OnRequest>& listener);
        static void Uninit();
        static std::shared_ptr<CarrierSessionHelper> Create();

    private:
        static std::weak_ptr<Carrier> CarrierHandler;
        static std::function<OnRequest> OnRequestListener;

        friend CarrierSessionHelper;
    };

    class ConnectListener {
    public:
        enum Notify : int8_t {
            Connected = 0,
            Closed,
            Error,
        };

        virtual void onNotify(Notify notify, int errCode) = 0;
        virtual void onReceivedData(const std::vector<uint8_t>& data) = 0;

        static const char* toString(Notify notify);
    };
    /*** static function and variable ***/

    /*** class function and variable ***/
    int allowConnectAsync(const std::string& peerId, std::shared_ptr<ConnectListener> listener);

    void disconnect();

    void setSdp(const std::string& sdp);

    int64_t sendData(const std::vector<uint8_t>& data);
    int64_t sendData(std::iostream& data);

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit CarrierSessionHelper() noexcept;
    virtual ~CarrierSessionHelper() noexcept;

    int makeSessionAndStream(const std::string& peerId);
    int replySession();
    int startSession();

    void connectNotify(ConnectListener::Notify notify, int errCode);

    std::shared_ptr<CarrierSession> sessionHandler;
    int sessionStreamId;
    std::shared_ptr<CarrierStreamCallbacks> sessionStreamCallbacks;
    std::string sessionSdp;
    std::shared_ptr<ThreadPool> threadPool; // avoid session thread pending when send mass data.
    std::shared_ptr<ConnectListener> connectListener;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _CARRIER_SESSION_HPP_ */


