#include "CarrierSessionHelper.hpp"

#include <functional>
#include <carrier.h>
#include <carrier_session.h>
#include <CommandHandler.hpp>
#include <DateTime.hpp>
#include <SafePtr.hpp>
#include <Semaphore.hpp>
#include <ThreadPool.hpp>

namespace trinity {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */
std::weak_ptr<Carrier> CarrierSessionHelper::Factory::CarrierHandler;
std::function<CarrierSessionHelper::Factory::OnRequest> CarrierSessionHelper::Factory::OnRequestListener;

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */
int CarrierSessionHelper::Factory::Init(
        std::weak_ptr<Carrier> carrier,
        const std::function<OnRequest>& listener)
{
    SAFE_GET_PTR(ptr, carrier);

    int ret = carrier_session_init(ptr.get());
    if (ret < 0) {
        CommandHandler::PrintCarrierError("Failed to new carrier session!");
        ret = ErrCode::CarrierSessionInitFailed;
    }
    CHECK_ERROR(ret);

    CarrierHandler = carrier;
    OnRequestListener = listener;

    auto onSessionRequest = [](
            Carrier *carrier, const char *from,
            const char *bundle, const char *sdp, size_t len, void *context)
    {
        Log::D(Log::Tag::Msg, "Carrier session request callback!");
        OnRequestListener(CarrierHandler, from, sdp);
    };
    ret = carrier_session_set_callback(ptr.get(), nullptr, onSessionRequest, nullptr);
    if (ret < 0) {
        CommandHandler::PrintCarrierError("Failed to set carrier session callback!");
        ret = ErrCode::CarrierSessionInitFailed;
    }
    CHECK_ERROR(ret);

    return 0;
}

void CarrierSessionHelper::Factory::Uninit()
{
    auto carrier = CarrierHandler.lock();
    if(carrier.get() != nullptr) {
        carrier_session_cleanup(carrier.get());
    }
    CarrierHandler.reset();
}

std::shared_ptr<CarrierSessionHelper> CarrierSessionHelper::Factory::Create()
{
    struct Impl: CarrierSessionHelper {
    };
    auto impl = std::make_shared<Impl>();

    return impl;
}

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */
void CarrierSessionHelper::setSdp(const std::string& sdp)
{
    this->sessionSdp = sdp;
}

int CarrierSessionHelper::allowConnectAsync(
        const std::string& peerId,
        std::shared_ptr<ConnectListener> listener)
{
    connectListener = listener;
    int ret = makeSessionAndStream(peerId);
    if(ret < 0) {
        this->connectNotify(ConnectListener::Notify::Error, ret);
    }
    CHECK_ERROR(ret);

    // session reply and start will process in state changed callback async

    return 0;
}

void CarrierSessionHelper::disconnect()
{
    if(sessionHandler == nullptr) {
        return;
    }

    carrier_session_remove_stream(sessionHandler.get(), sessionStreamId);
    sessionHandler.reset();

    sessionStreamId = -1;
    sessionSdp.clear();
    // sessionThread.reset();

    connectNotify(ConnectListener::Notify::Closed, 0);
    connectListener = nullptr;

    // sessionPeerId.clear();
}

int64_t CarrierSessionHelper::sendData(const std::vector<uint8_t>& data)
{
    Log::D(Log::Tag::Msg, "CarrierSessionHelper send vector data, len: %d", data.size());

    const int step = 2048;
    for(int idx = 0; idx < data.size(); idx+=step) {
        int sendSize = step < (data.size() - idx) ? step : (data.size() - idx);
        int ret = carrier_stream_write(sessionHandler.get(), sessionStreamId,
                                   data.data() + idx, sendSize);
        if (ret < 0) {
            CommandHandler::PrintCarrierError("Failed to send vector data through carrier session!");
            ret = ErrCode::CarrierSessionSendFailed;
        }
        CHECK_ERROR(ret);
    }

    return data.size();
}

int64_t CarrierSessionHelper::sendData(std::iostream& data)
{
    data.seekg(0, data.end);
    int dataSize = data.tellg();
    data.seekg(0, data.beg);
    Log::D(Log::Tag::Msg, "CarrierSessionHelper send stream data, len: %d", dataSize);

    const int step = 2048;
    uint8_t buf[step];
    for(int idx = 0; idx < dataSize; idx+=step) {
        int sendSize = step < (dataSize - idx) ? step : (dataSize - idx);
        data.read(reinterpret_cast<char*>(buf), sendSize);

        int ret = carrier_stream_write(sessionHandler.get(), sessionStreamId,
                                   buf, sendSize);
        if (ret < 0) {
            CommandHandler::PrintCarrierError("Failed to send stream data through carrier session!");
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
CarrierSessionHelper::CarrierSessionHelper() noexcept
    : sessionHandler()
    , sessionStreamId(-1)
    , sessionSdp()
    , threadPool()
    , connectListener()
{
    threadPool = ThreadPool::Create("carrier-session");
}

CarrierSessionHelper::~CarrierSessionHelper() noexcept
{
    connectListener = nullptr; // fix dead loop issue.
    disconnect();
}

int CarrierSessionHelper::makeSessionAndStream(const std::string& peerId)
{
    SAFE_GET_PTR(carrier, Factory::CarrierHandler);

    auto creater = [&]() -> CarrierSession* {
        auto ptr = carrier_session_new(carrier.get(), peerId.c_str());
        return ptr;
    };
    auto deleter = [=](CarrierSession* ptr) -> void {
        if(ptr != nullptr) {
            carrier_session_close(ptr);
            Log::D(Log::Tag::Msg, "Destroy an ela carrier session with %s, stream %d", peerId.c_str(), sessionStreamId);
        }
    };
    sessionHandler = std::shared_ptr<CarrierSession>(creater(), deleter);
    if (sessionHandler == nullptr) {
        CommandHandler::PrintCarrierError("Failed to new carrier session!");
    }
    CHECK_ASSERT(sessionHandler != nullptr, ErrCode::CarrierSessionCreateFailed);

    auto onStateChanged = [](
            CarrierSession *session, int stream,
            CarrierStreamState state, void *context)
    {
        auto thiz = reinterpret_cast<CarrierSessionHelper*>(context);
        Log::D(Log::Tag::Msg, "CarrierSessionHelper state change to %d at session stream %d", state, stream);
        auto weakPtr = thiz->weak_from_this();
        auto carrierSession = weakPtr.lock();
        if(carrierSession == nullptr) {
            Log::D(Log::Tag::Msg, "CarrierSessionHelper has been released");
            return;
        }

        ConnectListener::Notify notify = static_cast<ConnectListener::Notify>(-1);
        int ret = 0;
        switch (state) {
        case CarrierStreamState_initialized:
            ret = carrierSession->replySession();
            if(ret < 0) {
                notify = ConnectListener::Notify::Error;
            }
            break;
        case CarrierStreamState_transport_ready:
            // wait for request complete if sdp is not set.
            if(carrierSession->sessionSdp.empty() == true) {
                break;
            }
            ret = carrierSession->startSession();
            if(ret < 0) {
                notify = ConnectListener::Notify::Error;
            }
            break;
        case CarrierStreamState_connected:
            notify = ConnectListener::Notify::Connected;
            break;
        case CarrierStreamState_closed:
            notify = ConnectListener::Notify::Closed;
            break;
        case CarrierStreamState_failed:
            notify = ConnectListener::Notify::Error;
            CommandHandler::PrintCarrierError("Carrier session state change to failed!");
            ret = ErrCode::CarrierSessionErrorExists;
            break;
        default:
            break;
        }

        if(notify >= 0) {
            carrierSession->connectNotify(notify, ret);
        }
    };
    auto onReceivedData = [](
            CarrierSession *session, int stream,
            const void *data, size_t len, void *context)
    {
        auto thiz = reinterpret_cast<CarrierSessionHelper*>(context);
        auto weakPtr = thiz->weak_from_this();
        SAFE_GET_PTR_NO_RETVAL(carrierSession, weakPtr);

        if(carrierSession->connectListener != nullptr) {
            auto dataCast = reinterpret_cast<const uint8_t*>(data);
            auto dataPtr = std::make_shared<std::vector<uint8_t>>(dataCast, dataCast + len);
            carrierSession->threadPool->post([=] {
                carrierSession->connectListener->onReceivedData(*dataPtr);
            });
        }
    };
    sessionStreamCallbacks = std::make_shared<CarrierStreamCallbacks>();
    sessionStreamCallbacks->state_changed = onStateChanged;
    sessionStreamCallbacks->stream_data = onReceivedData;
    int ret = carrier_session_add_stream(sessionHandler.get(),
                                     CarrierStreamType_application, ELA_STREAM_RELIABLE,
                                     sessionStreamCallbacks.get(), this);
    if (ret < 0) {
        CommandHandler::PrintCarrierError("Failed to add stream!");
        ret = ErrCode::CarrierSessionAddStreamFailed;
    }
    CHECK_ERROR(ret);
    sessionStreamId = ret;
    Log::D(Log::Tag::Msg, "Create a new ela carrier session with %s stream %d", peerId.c_str(), sessionStreamId);

    return 0;
}

int CarrierSessionHelper::replySession()
{
    int ret= carrier_session_reply_request(sessionHandler.get(), nullptr, 0, nullptr);
    if (ret < 0) {
        CommandHandler::PrintCarrierError("Failed to reply carrier session!");
        ret = ErrCode::CarrierSessionConnectFailed;
    }
    CHECK_ERROR(ret);

    return 0;
}

int CarrierSessionHelper::startSession()
{
    int ret = carrier_session_start(sessionHandler.get(), sessionSdp.c_str(), sessionSdp.length());
    if (ret < 0) {
        CommandHandler::PrintCarrierError("Failed to start carrier session!");
        ret = ErrCode::CarrierSessionStartFailed;
    }
    CHECK_ERROR(ret);

    return 0;
}

void CarrierSessionHelper::connectNotify(ConnectListener::Notify notify, int errCode)
{
    if(connectListener == nullptr) {
        return;
    }

    threadPool->post([=] {
        connectListener->onNotify(notify, errCode);
    });
}

const char* CarrierSessionHelper::ConnectListener::toString(ConnectListener::Notify notify)
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


} // namespace trinity