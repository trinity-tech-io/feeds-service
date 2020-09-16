#ifndef _CARRIER_SESSION_HPP_
#define _CARRIER_SESSION_HPP_

#include <future> 
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <Semaphore.hpp>

struct ElaCarrier;
struct ElaSession;

namespace elastos {

class ThreadPool;

class CarrierSession : public std::enable_shared_from_this<CarrierSession> {
public:
    /*** type define ***/
    class Factory {
    public:
        using OnRequest = void(std::weak_ptr<ElaCarrier> carrier,
                             const std::string& from, const std::string& sdp);

        static int Init(std::weak_ptr<ElaCarrier> carrier, const std::function<OnRequest>& listener);
        static void Uninit();
        static std::shared_ptr<CarrierSession> Create();

    private:
        static std::weak_ptr<ElaCarrier> CarrierHandler;
        static std::function<OnRequest> OnRequestListener;

        friend CarrierSession;
    };

    class ConnectListener {
    public:
        enum Notify {
            Connected,
            Closed,
            Error,
        };

        virtual void onNotify(Notify notify, int errCode) = 0;
        virtual void onReceivedData(const std::vector<uint8_t>& data) = 0;
    };
    /*** static function and variable ***/

    /*** class function and variable ***/
    int requestConnect(const std::string& peerId);
    int allowConnect(const std::string& peerId, std::shared_ptr<ConnectListener> listener);

    void disconnect();

    void setSdp(const std::string& sdp);
protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/
    struct State {
        int waitFor(int state, int timeout);
        void notify(int state);
        int get();
        void reset();
    private:
        int state;
        Semaphore semaphore;
    };

    /*** static function and variable ***/
    static void PrintElaCarrierError(const std::string& errReason);

    /*** class function and variable ***/
    explicit CarrierSession() noexcept;
    virtual ~CarrierSession() noexcept;

    int makeSessionAndStream(const std::string& peerId);
    void connectNotify(ConnectListener::Notify notify, int errCode);

    // std::string sessionPeerId;
    std::shared_ptr<ElaSession> sessionHandler;
    int sessionStreamId;
    std::string sessionSdp;
    // std::shared_ptr<ThreadPool> sessionThread;
    State state;
    std::shared_ptr<ConnectListener> connectListener;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace elastos

#endif /* _CARRIER_SESSION_HPP_ */


