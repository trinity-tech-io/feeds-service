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
int CarrierSession::Factory::Init(std::weak_ptr<ElaCarrier> carrier,
                                  const std::function<OnRequest>& listener) {
    auto ptr = SAFE_GET_PTR(carrier);

    int ret = ela_session_init(ptr.get());
    if (ret < 0) {
        PrintElaCarrierError("Failed to new carrier session!");
        ret = ErrCode::CarrierSessionInitFailed;
    }
    CHECK_ERROR(ret);

    CarrierHandler = carrier;
    OnRequestListener = listener;

    ret = ela_session_set_callback(ptr.get(), nullptr,
        [](ElaCarrier *carrier, const char *from,
            const char *bundle, const char *sdp, size_t len, void *context) {
            Log::W(Log::TAG, "Carrier session request callback!");
            OnRequestListener(CarrierHandler, from, sdp);            
        },
        nullptr);
    if (ret < 0) {
        PrintElaCarrierError("Failed to set carrier session callback!");
        ret = ErrCode::CarrierSessionInitFailed;
    }
    CHECK_ERROR(ret);

    return 0;
}

void CarrierSession::Factory::Uninit()
{
    Log::W(Log::TAG, "%s", __PRETTY_FUNCTION__);

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
int CarrierSession::requestConnect(const std::string& peerId)
{
    int ret = makeSessionAndStream(peerId);
    CHECK_ERROR(ret);
    bool timeout = state.waitFor(ElaStreamState_initialized, 1000);
    CHECK_ASSERT(timeout == false, ErrCode::CarrierSessionTimeoutError);

    ret = ela_session_request(sessionHandler.get(), nullptr, 
                              [] (ElaSession *session,
                                  const char *bundle, int status, const char *reason,
                                  const char *sdp, size_t len, void *context) {
                                    Log::D(Log::TAG, "Carrier Session request completed");
                                    auto ptr = reinterpret_cast<CarrierSession*>(context);
                                    auto thiz = ptr->shared_from_this();
                                    if(thiz == nullptr) {
                                        Log::E(Log::TAG, "Carrier Session has been released");
                                    }
                                    thiz->setSdp(sdp);

                                    ElaStreamState state;
                                    ela_stream_get_state(thiz->sessionHandler.get(), thiz->sessionStreamId, &state);
                                    if (state == ElaStreamState_transport_ready) {
                                        thiz->state.notify( ElaStreamState_transport_ready);
                                    }
                                } , nullptr);
    timeout = state.waitFor(ElaStreamState_transport_ready, 10000);
    CHECK_ASSERT(timeout == false, ErrCode::CarrierSessionTimeoutError);

    ElaStreamState elsState;
    ela_stream_get_state(sessionHandler.get(), sessionStreamId, &elsState);
    if(elsState == ElaStreamState_transport_ready && sessionSdp.empty() == false) {
        ela_session_start(sessionHandler.get(), sessionSdp.c_str(), sessionSdp.length());
    }

    timeout = state.waitFor(ElaStreamState_connected, 10000);
    CHECK_ASSERT(timeout == false, ErrCode::CarrierSessionTimeoutError);

    connectNotify(ConnectListener::Notify::Connected, 0);

    return 0;
}

int CarrierSession::allowConnect(const std::string& peerId, std::shared_ptr<ConnectListener> listener)
{
    Log::D(Log::TAG, "%s start", __PRETTY_FUNCTION__);
    // sessionPeerId = peerId;
    connectListener = listener;

    int ret = makeSessionAndStream(peerId);
    CHECK_ERROR(ret);
    ret = state.waitFor(ElaStreamState_initialized, 5000);
    CHECK_ERROR(ret);
    Log::D(Log::TAG, "%s %d", __PRETTY_FUNCTION__, __LINE__);

    ret = ela_session_reply_request(sessionHandler.get(), nullptr, 0, nullptr);
    if (ret < 0) {
        PrintElaCarrierError("Failed to reply carrier session!");
        ret = ErrCode::CarrierSessionReplyFailed;
    }
    CHECK_ERROR(ret);
    ret = state.waitFor(ElaStreamState_transport_ready, 10000);
    CHECK_ERROR(ret);
    Log::D(Log::TAG, "%s %d", __PRETTY_FUNCTION__, __LINE__);

    ret = ela_session_start(sessionHandler.get(), sessionSdp.c_str(), sessionSdp.length());
    if (ret < 0) {
        PrintElaCarrierError("Failed to start carrier session!");
        ret = ErrCode::CarrierSessionStartFailed;
    }
    CHECK_ERROR(ret);

    ret = state.waitFor(ElaStreamState_connected, 10000);
    CHECK_ERROR(ret);
    Log::D(Log::TAG, "%s %d", __PRETTY_FUNCTION__, __LINE__);

    connectNotify(ConnectListener::Notify::Connected, 0);

    Log::D(Log::TAG, "%s end", __PRETTY_FUNCTION__);
    return 0;
}

void CarrierSession::disconnect()
{
    if(sessionHandler == nullptr) {
        return;
    }

    sessionHandler.reset();

    sessionStreamId = -1;
    sessionSdp.clear();
    // sessionThread.reset();

    state.notify(ElaStreamState_closed);
    state.reset();

    connectNotify(ConnectListener::Notify::Closed, 0);
    connectListener = nullptr;

    // sessionPeerId.clear();
}

int64_t CarrierSession::sendData(const std::vector<uint8_t>& data)
{
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
    // , sessionThread()
    , state()
    , connectListener()
{
    Log::D(Log::TAG, "%s", __PRETTY_FUNCTION__);
}

CarrierSession::~CarrierSession() noexcept
{
    Log::D(Log::TAG, "%s", __PRETTY_FUNCTION__);
    connectListener = nullptr; // fix dead loop issue.
    disconnect();
    Log::D(Log::TAG, "%s", __PRETTY_FUNCTION__);
}

int CarrierSession::makeSessionAndStream(const std::string& peerId)
{
    Log::D(Log::TAG, "%s start", __PRETTY_FUNCTION__);
    auto carrier = SAFE_GET_PTR(Factory::CarrierHandler);

    auto creater = [&]() -> ElaSession* {
        Log::I(Log::TAG, "Create ela carrier session");
        auto ptr = ela_session_new(carrier.get(), peerId.c_str());
        return ptr;
    };
    auto deleter = [=](ElaSession* ptr) -> void {
        Log::I(Log::TAG, "CarrierSession::makeSessionAndStream() destroy ela carrier session");
        if(ptr != nullptr) {
            ela_session_close(ptr);
        }
    };
    sessionHandler = std::shared_ptr<ElaSession>(creater(), deleter);
    if (sessionHandler == nullptr) {
        PrintElaCarrierError("Failed to new carrier session!");
    }
    CHECK_ASSERT(sessionHandler, ErrCode::CarrierSessionCreateFailed);

    ElaStreamCallbacks streamCallbacks = {
        .state_changed = [] (ElaSession *session, int stream,
                             ElaStreamState state,
                             void *context) {
            Log::D(Log::TAG, "Carrier Session state change to %d", state);
            auto ptr = reinterpret_cast<CarrierSession*>(context);
            auto thiz = ptr->shared_from_this();
            if(thiz == nullptr) {
                Log::E(Log::TAG, "Carrier Session has been released");
                return;
            }

            if (state == ElaStreamState_transport_ready && thiz->sessionSdp.empty()) { // notify at request completed
                return;
            }
            thiz->state.notify(state);

            if(state == ElaStreamState_closed) {
                thiz->connectNotify(ConnectListener::Notify::Closed, 0);
            } else if(state == ElaStreamState_failed) {
                thiz->connectNotify(ConnectListener::Notify::Error, -1); // TODO: err code
            }
        },
        .stream_data = [] (ElaSession *session, int stream,
                           const void *data, size_t len,
                           void *context) {
            Log::D(Log::TAG, "Carrier Session received data, len: %d", len);
            auto ptr = reinterpret_cast<CarrierSession*>(context);
            auto thiz = ptr->shared_from_this();
            if(thiz == nullptr) {
                Log::E(Log::TAG, "Carrier Session has been released");
                return;
            }

            if(thiz->connectListener != nullptr) {
                auto dataCast = reinterpret_cast<const uint8_t*>(data);
                auto dataPtr = std::vector<uint8_t>(dataCast, dataCast + len);
                thiz->connectListener->onReceivedData(dataPtr);
            }
        },
    };

    int ret = ela_session_add_stream(sessionHandler.get(),
                                     ElaStreamType_application, ELA_STREAM_RELIABLE,
                                     &streamCallbacks, this);
    if (ret < 0) {
        PrintElaCarrierError("Failed to add stream!");
        ret = ErrCode::CarrierSessionAddStreamFailed;
    }
    CHECK_ERROR(ret);
    sessionStreamId = ret;

    Log::D(Log::TAG, "%s end", __PRETTY_FUNCTION__);
    return 0;
}

void CarrierSession::connectNotify(ConnectListener::Notify notify, int errCode)
{
    if(connectListener == nullptr) {
        return;
    }

    connectListener->onNotify(notify, errCode);
}

int CarrierSession::State::waitFor(int state, int timeout)
{
    auto now = DateTime::CurrentMS();

    do {
        auto waittime = (timeout - (DateTime::CurrentMS() - now));
        auto ret = semaphore.waitfor(waittime);
        CHECK_ASSERT(ret, ErrCode::CarrierSessionTimeoutError);

        if(state >= ElaStreamState_deactivated) {
            CHECK_ERROR(ErrCode::CarrierSessionBadStatus);
        }
    } while(state != this->state);

    return 0;
}

void CarrierSession::State::notify(int state)
{
    this->state = state;
    semaphore.notify();
}

int CarrierSession::State::get()
{
    return state;
}

void CarrierSession::State::reset()
{
    state = 0;
}

} // namespace elastos