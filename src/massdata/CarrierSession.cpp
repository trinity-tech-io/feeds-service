#include "CarrierSession.hpp"

#include <functional>
#include <ela_carrier.h>
#include <ela_session.h>
#include <DateTime.hpp>
#include <SafePtr.hpp>
#include <Semaphore.hpp>
#include <ThreadPool.hpp>

namespace elastos {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */
std::weak_ptr<ElaCarrier> CarrierSession::Factory::CarrierHandler;
std::function<CarrierSession::Factory::OnRequest> CarrierSession::Factory::OnRequestListener;

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */
int CarrierSession::Factory::Init(
        std::weak_ptr<ElaCarrier> carrier,
        const std::function<OnRequest>& listener)
{
    Log::V(Log::TAG, "%s", __PRETTY_FUNCTION__);
    auto ptr = SAFE_GET_PTR(carrier);

    int ret = ela_session_init(ptr.get());
    if (ret < 0) {
        PrintElaCarrierError("Failed to new carrier session!");
        ret = ErrCode::CarrierSessionInitFailed;
    }
    CHECK_ERROR(ret);

    CarrierHandler = carrier;
    OnRequestListener = listener;

    auto onSessionRequest = 
        [](ElaCarrier *carrier, const char *from,
            const char *bundle, const char *sdp, size_t len, void *context) {
            Log::D(Log::TAG, "Carrier session request callback!");
            OnRequestListener(CarrierHandler, from, sdp);            
        };
    ret = ela_session_set_callback(ptr.get(), nullptr, onSessionRequest, nullptr);
    if (ret < 0) {
        PrintElaCarrierError("Failed to set carrier session callback!");
        ret = ErrCode::CarrierSessionInitFailed;
    }
    CHECK_ERROR(ret);

    return 0;
}

void CarrierSession::Factory::Uninit()
{
    Log::V(Log::TAG, "%s", __PRETTY_FUNCTION__);

    auto carrier = CarrierHandler.lock();
    if(carrier.get() != nullptr) {
        ela_session_cleanup(carrier.get());
    }
    CarrierHandler.reset();
}

std::shared_ptr<CarrierSession> CarrierSession::Factory::Create()
{
    struct Impl: CarrierSession {
    };
    auto impl = std::make_shared<Impl>();

    return impl;
}

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */
void CarrierSession::setSdp(const std::string& sdp)
{
    this->sessionSdp = sdp;
}

// NOT TEST
int CarrierSession::requestConnectAsync(
        const std::string &peerId,
        std::shared_ptr<ConnectListener> listener)
{
    acitveRequest = true;
    connectListener = listener;
    int ret = makeSessionAndStream(peerId);
    if(ret < 0) {
        this->connectNotify(ConnectListener::Notify::Error, ret);
    }
    CHECK_ERROR(ret);

    // session request and start will process in state changed callback async

    return 0;
}

int CarrierSession::allowConnectAsync(
        const std::string& peerId,
        std::shared_ptr<ConnectListener> listener)
{
    Log::V(Log::TAG, "%s", __PRETTY_FUNCTION__);

    acitveRequest = false;
    connectListener = listener;
    int ret = makeSessionAndStream(peerId);
    if(ret < 0) {
        this->connectNotify(ConnectListener::Notify::Error, ret);
    }
    CHECK_ERROR(ret);

    // session reply and start will process in state changed callback async

    return 0;
}

void CarrierSession::disconnect()
{
    if(sessionHandler == nullptr) {
        return;
    }

    ela_session_remove_stream(sessionHandler.get(), sessionStreamId);
    sessionHandler.reset();

    sessionStreamId = -1;
    sessionSdp.clear();
    // sessionThread.reset();

    connectNotify(ConnectListener::Notify::Closed, 0);
    connectListener = nullptr;

    // sessionPeerId.clear();
}

int64_t CarrierSession::sendData(const std::vector<uint8_t>& data)
{
    Log::D(Log::TAG, "CarrierSession send vector data, len: %d", data.size());

    const int step = 1024;
    for(int idx = 0; idx < data.size(); idx+=step) {
        int sendSize = step < (data.size() - idx) ? step : (data.size() - idx);
        int ret = ela_stream_write(sessionHandler.get(), sessionStreamId,
                                   data.data() + idx, sendSize);
        if (ret < 0) {
            PrintElaCarrierError("Failed to send vector data through carrier session!");
            ret = ErrCode::CarrierSessionSendFailed;
        }
        CHECK_ERROR(ret);
    }

    return data.size();
}

int64_t CarrierSession::sendData(std::iostream& data)
{
    data.seekg(0, data.end);
    int dataSize = data.tellg();
    data.seekg(0, data.beg);
    Log::D(Log::TAG, "CarrierSession send stream data, len: %d", dataSize);

    const int step = 1024;
    uint8_t buf[step];
    for(int idx = 0; idx < dataSize; idx+=step) {
        int sendSize = step < (dataSize - idx) ? step : (dataSize - idx);
        data.read(reinterpret_cast<char*>(buf), sendSize);

        int ret = ela_stream_write(sessionHandler.get(), sessionStreamId,
                                   buf, sendSize);
        if (ret < 0) {
            PrintElaCarrierError("Failed to send stream data through carrier session!");
            ret = ErrCode::CarrierSessionSendFailed;
        }
        CHECK_ERROR(ret);
    }

    return dataSize;
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
void CarrierSession::PrintElaCarrierError(const std::string& errReason)
{
    int errCode = ela_get_error();

    char errStr[1024] = {0};
    ela_get_strerror(errCode, errStr, sizeof(errStr));

        Log::E(Log::TAG, "%s carrier error desc is %s(0x%x)",
                         errReason.c_str(), errStr, errCode);
}

CarrierSession::CarrierSession() noexcept
    : sessionHandler()
    , sessionStreamId(-1)
    , sessionSdp()
    , threadPool()
    , connectListener()
    , acitveRequest(false)
{
    threadPool = std::make_shared<ThreadPool>("carrier-session");
    Log::V(Log::TAG, "%s", __PRETTY_FUNCTION__);
}

CarrierSession::~CarrierSession() noexcept
{
    connectListener = nullptr; // fix dead loop issue.
    disconnect();
    Log::V(Log::TAG, "%s", __PRETTY_FUNCTION__);
}

int CarrierSession::makeSessionAndStream(const std::string& peerId)
{
    Log::V(Log::TAG, "%s start", __PRETTY_FUNCTION__);
    auto carrier = SAFE_GET_PTR(Factory::CarrierHandler);

    auto creater = [&]() -> ElaSession* {
        auto ptr = ela_session_new(carrier.get(), peerId.c_str());
        Log::I(Log::TAG, "Create a new ela carrier session at %p", ptr);
        return ptr;
    };
    auto deleter = [=](ElaSession* ptr) -> void {
        if(ptr != nullptr) {
            ela_session_close(ptr);
        }
        Log::I(Log::TAG, "Destroy an ela carrier session at %p", ptr);
    };
    sessionHandler = std::shared_ptr<ElaSession>(creater(), deleter);
    if (sessionHandler == nullptr) {
        PrintElaCarrierError("Failed to new carrier session!");
    }
    CHECK_ASSERT(sessionHandler, ErrCode::CarrierSessionCreateFailed);

    auto onStateChanged =
        [](ElaSession *session, int stream,
           ElaStreamState state, void *context) {
            auto thiz = reinterpret_cast<CarrierSession*>(context);
            Log::V(Log::TAG, "CarrierSession state change to %d, session=%p,stream=%d", state, session, stream);
            auto weakPtr = thiz->weak_from_this();
            auto carrierSession = weakPtr.lock();
            if(carrierSession == nullptr) {
                Log::D(Log::TAG, "CarrierSession has been released");
                return;
            }

            ConnectListener::Notify notify = static_cast<ConnectListener::Notify>(-1);
            int ret = 0;
            switch (state) {
            case ElaStreamState_initialized:
                ret = carrierSession->requestOrReplySession();
                if(ret < 0) {
                    notify = ConnectListener::Notify::Error;
                }
                break;
            case ElaStreamState_transport_ready:
                // wait for request complete if sdp is not set.
                if(carrierSession->sessionSdp.empty() == true) {
                    break;
                }
                ret = carrierSession->startSession();
                if(ret < 0) {
                    notify = ConnectListener::Notify::Error;
                }
                break;
            case ElaStreamState_connected:
                notify = ConnectListener::Notify::Connected;
                break;
            case ElaStreamState_closed:
                notify = ConnectListener::Notify::Closed;
                break;
            case ElaStreamState_failed:
                notify = ConnectListener::Notify::Error;
                ret = ErrCode::CarrierSessionErrorExists;
                break;
            default:
                break;
            }

            if(notify >= 0) {
                carrierSession->connectNotify(notify, ret);
            }
        };
    auto onReceivedData =
        [](ElaSession *session, int stream,
           const void *data, size_t len,
           void *context) {
            // Log::D(Log::TAG, "CarrierSession received data, len: %d", len);
            auto thiz = reinterpret_cast<CarrierSession*>(context);
            auto weakPtr = thiz->weak_from_this();
            auto carrierSession = SAFE_GET_PTR_NO_RETVAL(weakPtr);

            if(carrierSession->connectListener != nullptr) {
                auto dataCast = reinterpret_cast<const uint8_t*>(data);
                auto dataPtr = std::make_shared<std::vector<uint8_t>>(dataCast, dataCast + len);
                carrierSession->threadPool->post([=] {
                    carrierSession->connectListener->onReceivedData(*dataPtr);
                });
            }
        };
    sessionStreamCallbacks = std::make_shared<ElaStreamCallbacks>();
    sessionStreamCallbacks->state_changed = onStateChanged;
    sessionStreamCallbacks->stream_data = onReceivedData;
    int ret = ela_session_add_stream(sessionHandler.get(),
                                     ElaStreamType_application, ELA_STREAM_RELIABLE,
                                     sessionStreamCallbacks.get(), this);
    Log::D(Log::TAG, "CarrierSession add stream to carrier session=%p,stream=%d", sessionHandler.get(), ret);
    if (ret < 0) {
        PrintElaCarrierError("Failed to add stream!");
        ret = ErrCode::CarrierSessionAddStreamFailed;
    }
    CHECK_ERROR(ret);
    sessionStreamId = ret;

    Log::V(Log::TAG, "%s end", __PRETTY_FUNCTION__);
    return 0;
}

int CarrierSession::requestOrReplySession()
{
    int ret;

    if(acitveRequest == true) { // TODO: not test
        auto onRequestComplete =
            [](ElaSession *session, const char *bundle,
               int status, const char *reason,
               const char *sdp, size_t len, void *context) {
                Log::D(Log::TAG, "CarrierSession request completed");
                auto thiz = reinterpret_cast<CarrierSession*>(context);
                auto weakPtr = thiz->weak_from_this();
                auto carrierSession = SAFE_GET_PTR_NO_RETVAL(weakPtr);

                carrierSession->setSdp(sdp);

                ElaStreamState state;
                ela_stream_get_state(carrierSession->sessionHandler.get(), carrierSession->sessionStreamId, &state);
                if (state != ElaStreamState_transport_ready) { // wait for session ready
                    return;
                }

                int ret = carrierSession->startSession();
                if (ret < 0) {
                    carrierSession->connectNotify( ConnectListener::Notify::Error, ret);
                }
        };

        ret = ela_session_request(sessionHandler.get(), nullptr, 
                                  onRequestComplete, nullptr);
    } else {
        ret = ela_session_reply_request(sessionHandler.get(), nullptr, 0, nullptr);
    }
    if (ret < 0) {
        PrintElaCarrierError("Failed to try to connect carrier session!");
        ret = ErrCode::CarrierSessionConnectFailed;
    }
    CHECK_ERROR(ret);

    return 0;
}

int CarrierSession::startSession()
{
    int ret = ela_session_start(sessionHandler.get(), sessionSdp.c_str(), sessionSdp.length());
    if (ret < 0) {
        PrintElaCarrierError("Failed to start carrier session!");
        ret = ErrCode::CarrierSessionStartFailed;
    }
    CHECK_ERROR(ret);

    return 0;
}

void CarrierSession::connectNotify(ConnectListener::Notify notify, int errCode)
{
    if(connectListener == nullptr) {
        return;
    }

    threadPool->post([=] {
        connectListener->onNotify(notify, errCode);
    });
}

const char* CarrierSession::ConnectListener::toString(ConnectListener::Notify notify)
{
    const char* notifyStr[] = {
        "Connected",
        "Closed",
        "Error",
    };

    auto idx = static_cast<int>(notify);
    int size = sizeof(notifyStr) / sizeof(*notifyStr);
    if(idx >= size) {
        return "Unknown";
    }

    return notifyStr[idx];
}


} // namespace elastos