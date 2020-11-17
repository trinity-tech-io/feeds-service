#include "CommandHandler.hpp"

#include <SafePtr.hpp>
#include <LegacyMethod.hpp>
#include <MassData.hpp>
#include <ThreadPool.hpp>

#include <crystal.h>
extern "C" {
#define new fix_cpp_keyword_new
#include <auth.h>
#include <did.h>
#undef new
}

namespace trinity {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */
std::shared_ptr<CommandHandler> CommandHandler::CmdHandlerInstance;

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */
std::shared_ptr<CommandHandler> CommandHandler::GetInstance()
{
    // ignore check thread-safty, no needed

    if(CmdHandlerInstance != nullptr) {
        return CmdHandlerInstance;
    }

    struct Impl: CommandHandler {
    };
    CmdHandlerInstance = std::make_shared<Impl>();

    return CmdHandlerInstance;
}


/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */
int CommandHandler::config(const std::filesystem::path& dataDir,
                           std::weak_ptr<ElaCarrier> carrier)
{
    threadPool = std::make_shared<ThreadPool>("command-handler");
    carrierHandler = carrier;

    cmdListener = std::move(std::vector<std::shared_ptr<Listener>> {
        std::make_shared<LegacyMethod>(),
        std::make_shared<MassData>(dataDir / MassData::MassDataDirName),
    });

    return 0;
}

void CommandHandler::cleanup()
{
    CmdHandlerInstance.reset();

    threadPool.reset();
    carrierHandler.reset();
    cmdListener.clear();
}

std::weak_ptr<ElaCarrier> CommandHandler::getCarrierHandler()
{
    return carrierHandler;
}

int CommandHandler::processAsync(const std::string& from, const std::vector<uint8_t>& data)
{
    CHECK_ASSERT(threadPool, ErrCode::PointerReleasedError);

    threadPool->post([this, from = std::move(from), data = std::move(data)] {
        process(from, data);
    });

    return 0;
}

int CommandHandler::process(const std::string& from, const std::vector<uint8_t>& data)
{
    std::shared_ptr<Req> req;
    std::shared_ptr<Resp> resp;
    int ret = unpackRequest(data, req);
    if(ret >= 0) {
        Log::D(Log::TAG, "Command handler dispose method:%s, tsx_id:%llu, from:%s", req->method, req->tsx_id, from.c_str());
        ret = ErrCode::UnimplementedError;
        for (const auto &it : cmdListener) {
            ret = it->onDispose(from, req, resp);
            if (ret != ErrCode::UnimplementedError) {
                break;
            }
        }
        if(ret == ErrCode::CompletelyFinishedNotify) { // return if process totally finished.
            return 0;
        }
    }

    auto errCode = ret;
    std::vector<uint8_t> respData;
    ret = packResponse(req, resp, errCode, respData);
    CHECK_ERROR(ret);

    Marshalled marshalledResp = {
        respData.data(),
        respData.size()
    };
    msgq_enq(from.c_str(), &marshalledResp);

    return 0;
}

int CommandHandler::unpackRequest(const std::vector<uint8_t>& data,
                                  std::shared_ptr<Req>& req) const
{
    Req *reqBuf = nullptr;
    int ret = rpc_unmarshal_req(data.data(), data.size(), &reqBuf);
    auto deleter = [](void* ptr) -> void {
        deref(ptr);
    };
    req = std::shared_ptr<Req>(reqBuf, deleter); // workaround: declare for auto release Req pointer
    if (ret == -1) {
        ret = ErrCode::MassDataUnmarshalReqFailed;
    } else if (ret == -2) {
        ret = ErrCode::MassDataUnknownReqFailed;
    } else if (ret == -3) {
        ret = ErrCode::MassDataUnsupportedVersion;
    } else if(ret < ErrCode::StdSystemErrorIndex) {
        ret = ErrCode::StdSystemError;
    } else if (ret < 0) {
        ret = ErrCode::UnknownError;
    }
    CHECK_ERROR(ret);

    return 0;
}

int CommandHandler::packResponse(const std::shared_ptr<Req> &req,
                                 const std::shared_ptr<Resp> &resp,
                                 int errCode,
                                 std::vector<uint8_t> &data) const
{
    Marshalled* marshalBuf = nullptr;
    if(errCode >= 0) {
        CHECK_ASSERT(resp, ErrCode::MassDataUnknownRespFailed);
        marshalBuf = rpc_marshal_resp(req->method, resp.get());
    } else {
        CHECK_ASSERT(req, ErrCode::MassDataUnknownReqFailed);
        auto errDesp = ErrCode::ToString(errCode);
        marshalBuf = rpc_marshal_err(req->tsx_id, errCode, errDesp.c_str());
        Log::D(Log::TAG, "Response error:");
        Log::D(Log::TAG, "    code: %d", errCode);
        Log::D(Log::TAG, "    message: %s", errDesp.c_str());
    }
    auto deleter = [](void* ptr) -> void {
        deref(ptr);
    };
    auto marshalData = std::shared_ptr<Marshalled>(marshalBuf, deleter); // workaround: declare for auto release Marshalled pointer
    CHECK_ASSERT(marshalData, ErrCode::MassDataMarshalRespFailed);

    auto marshalDataPtr = reinterpret_cast<uint8_t*>(marshalData->data);
    data = {marshalDataPtr, marshalDataPtr + marshalData->sz};

    return 0;
}

void CommandHandler::Listener::setHandleMap(const std::map<const char*, Handler>& handleMap)
{
    cmdHandleMap = std::move(handleMap);
}

int CommandHandler::Listener::checkAccessible(Accessible accessible, const std::string &accessToken)
{
    int ret = ErrCode::UnknownError;

#ifdef NDEBUG
    Log::D(Log::TAG, "Checking request accessible.");
    if (accessible == Accessible::Owner) {
        ret = isOwner(accessToken);
    } else if (accessible == Accessible::Member) {
        ret = isMember(accessToken);
    } else { // accessible is Any
        ret = 0;
    }
    CHECK_ERROR(ret);
#else
    Log::W(Log::TAG, "Debug: Ignore to check request accessible.");
#endif

    return 0;
}

int CommandHandler::Listener::onDispose(const std::string& from,
                                        std::shared_ptr<Req> req,
                                        std::shared_ptr<Resp> &resp)
{
    std::ignore = from;

    for (const auto& it : cmdHandleMap) {
        if (std::strcmp(it.first, req->method) != 0) {
            continue;
        }

        int ret = checkAccessible(it.second.accessible, reinterpret_cast<TkReq*>(req.get())->params.tk);
        CHECK_ERROR(ret);

        ret = it.second.callback(req, resp);
        CHECK_ERROR(ret);

        return ret;
    }

    return ErrCode::UnimplementedError;
}

int CommandHandler::Listener::isOwner(const std::string& accessToken)
{
    std::shared_ptr<UserInfo> userInfo;
    int ret = getUserInfo(accessToken, userInfo);
    CHECK_ERROR(ret);

    if (user_id_is_owner(userInfo->uid) == false) {
        Log::E(Log::TAG, "Mass data processor: request access while not being owner.");
        CHECK_ERROR(ErrCode::NotAuthorizedError);
    }

    return 0;
}

int CommandHandler::Listener::isMember(const std::string& accessToken)
{
    std::shared_ptr<UserInfo> userInfo;
    int ret = getUserInfo(accessToken, userInfo);
    CHECK_ERROR(ret);

    return 0;
}

int CommandHandler::Listener::getUserInfo(const std::string& accessToken, std::shared_ptr<UserInfo>& userInfo)
{
    if (did_is_ready() == false) {
        Log::E(Log::TAG, "Mass data processor: Feeds DID is not ready.");
        CHECK_ERROR(ErrCode::DidNotReady);
    }

    auto creater = [&]() -> UserInfo* {
        auto ptr = create_uinfo_from_access_token(accessToken.c_str());
        return ptr;
    };
    auto deleter = [=](UserInfo* ptr) -> void {
        deref(ptr);
    };
    userInfo = std::shared_ptr<UserInfo>(creater(), deleter);
    if (userInfo == nullptr) {
        Log::E(Log::TAG, "Mass data processor: Invalid access token.");
        CHECK_ERROR(ErrCode::InvalidAccessToken);
    }

    return 0;
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */

} // namespace trinity