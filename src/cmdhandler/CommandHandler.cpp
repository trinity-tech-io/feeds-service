#include "CommandHandler.hpp"

#include <cstring>
#include <ChannelMethod.hpp>
#include <LegacyMethod.hpp>
#include <MassData.hpp>
#include <SafePtr.hpp>
#include <StandardAuth.hpp>
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
std::filesystem::path CommandHandler::Listener::DataDir;

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
    int ret = Listener::SetDataDir(dataDir);
    CHECK_ERROR(ret);

    threadPool = std::make_shared<ThreadPool>("command-handler");
    carrierHandler = carrier;

    cmdListener = std::move(std::vector<std::shared_ptr<Listener>> {
        std::make_shared<LegacyMethod>(),
        std::make_shared<ChannelMethod>(),
        std::make_shared<MassData>(dataDir / MassData::MassDataDirName),
        std::make_shared<StandardAuth>(),
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
    CHECK_ASSERT(threadPool != nullptr, ErrCode::PointerReleasedError);

    threadPool->post([this, from = std::move(from), data = std::move(data)] {
        int ret = processAdvance(from, data);
        if(ret != ErrCode::UnimplementedError) {
            return;
        }

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
        for (const auto& it : cmdListener) {
            ret = it->onDispose(from, req, resp);
            if (ret != ErrCode::UnimplementedError) {
                break;
            }
        }
        if(ret == ErrCode::UnimplementedError) { // return if unimplemented.
            return ret;
        } else if(ret == ErrCode::CompletelyFinishedNotify) { // return if process totally finished.
            return 0;
        }
    }

    auto errCode = ret;
    std::vector<uint8_t> respData;
    ret = packResponse(req, resp, errCode, respData);
    CHECK_ERROR(ret);

    Marshalled* marshalledResp = (Marshalled*)rc_zalloc(sizeof(Marshalled) + respData.size(), NULL);
    marshalledResp->data = marshalledResp + 1;
    marshalledResp->sz = respData.size();
    memcpy(marshalledResp->data, respData.data(), respData.size());

    msgq_enq(from.c_str(), marshalledResp);
    deref(marshalledResp);

    return 0;
}

int CommandHandler::processAdvance(const std::string& from, const std::vector<uint8_t>& data)
{
    std::shared_ptr<Rpc::Request> request;
    std::vector<std::shared_ptr<Rpc::Response>> responseArray;

    int ret = Rpc::Factory::Unmarshal(data, request);
    if(ret == ErrCode::UnimplementedError) {
        return ret;
    }
    CHECK_ERROR(ret);

    for (const auto& it : cmdListener) {
        ret = it->onDispose(request, responseArray);
        if (ret != ErrCode::UnimplementedError) {
            break;
        }
    }

    for (const auto &response : responseArray) {
        auto errCode = ret;
        std::vector<uint8_t> respData;
        int ret = Rpc::Factory::Marshal(response, respData);
        CHECK_ERROR(ret);

        Marshalled* marshalledResp = (Marshalled*)rc_zalloc(sizeof(Marshalled) + respData.size(), NULL);
        marshalledResp->data = marshalledResp + 1;
        marshalledResp->sz = respData.size();
        memcpy(marshalledResp->data, respData.data(), respData.size());

        msgq_enq(from.c_str(), marshalledResp);
        deref(marshalledResp);
    }

    return 0;
}

int CommandHandler::unpackRequest(const std::vector<uint8_t>& data,
                                  std::shared_ptr<Req>& req) const
{
    Req *reqBuf = nullptr;
    int ret = rpc_unmarshal_req(data.data(), data.size(),& reqBuf);
    auto deleter = [](void* ptr) -> void {
        deref(ptr);
    };
    req = std::shared_ptr<Req>(reqBuf, deleter); // workaround: declare for auto release Req pointer
    if (ret == -1) {
        ret = ErrCode::CmdUnmarshalReqFailed;
    } else if (ret == -2) {
        ret = ErrCode::CmdUnknownReqFailed;
    } else if (ret == -3) {
        ret = ErrCode::CmdUnsupportedVersion;
    } else if(ret < ErrCode::StdSystemErrorIndex) {
        ret = ErrCode::StdSystemError;
    } else if (ret < 0) {
        ret = ErrCode::UnknownError;
    }
    if(ret < 0) {
        Log::W(Log::TAG, "Failed to unmarshal request: %s", data.data());
    }
    CHECK_ERROR(ret);

    return 0;
}

int CommandHandler::packResponse(const std::shared_ptr<Req>& req,
                                 const std::shared_ptr<Resp>& resp,
                                 int errCode,
                                 std::vector<uint8_t>& data) const
{
    Marshalled* marshalBuf = nullptr;
    if(errCode >= 0) {
        CHECK_ASSERT(resp != nullptr, ErrCode::CmdUnknownRespFailed);
        marshalBuf = rpc_marshal_resp(req->method, resp.get());
    } else {
        CHECK_ASSERT(req != nullptr, ErrCode::CmdUnknownReqFailed);
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
    CHECK_ASSERT(marshalData != nullptr, ErrCode::CmdMarshalRespFailed);

    auto marshalDataPtr = reinterpret_cast<uint8_t*>(marshalData->data);
    data = {marshalDataPtr, marshalDataPtr + marshalData->sz};

    return 0;
}

const std::filesystem::path& CommandHandler::Listener::GetDataDir()
{
    return DataDir;
}

int CommandHandler::Listener::SetDataDir(const std::filesystem::path& dataDir)
{
    auto dirExists = std::filesystem::exists(dataDir);
    if(dirExists == false) {
        dirExists = std::filesystem::create_directories(dataDir);
    }
    CHECK_ASSERT(dirExists, ErrCode::FileNotExistsError);

    DataDir = dataDir;

    return 0;
}

void CommandHandler::Listener::setHandleMap(const std::map<const char*, NormalHandler>& normalHandlerMap,
                                            const std::map<const char*, AdvancedHandler>& advancedHandlerMap)
{
    this->normalHandlerMap = std::move(normalHandlerMap);
    this->advancedHandlerMap = std::move(advancedHandlerMap);
}

int CommandHandler::Listener::checkAccessible(Accessible accessible, const std::string& accessToken)
{

#ifdef NDEBUG
    int ret = ErrCode::UnknownError;
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
                                        std::shared_ptr<Resp>& resp)
{
    std::ignore = from;

    for (const auto& it : normalHandlerMap) {
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

int CommandHandler::Listener::onDispose(std::shared_ptr<Rpc::Request> request,
                                        std::vector<std::shared_ptr<Rpc::Response>>& responseArray)
{
    for (const auto& it : advancedHandlerMap) {
        if (it.first != request->method) {
            continue;
        }

        Log::D(Log::TAG, "Request:");
        Log::D(Log::TAG, "  ->  %s", request->str().c_str());

        std::string accessToken;
        auto requestTokenPtr = std::dynamic_pointer_cast<Rpc::RequestWithToken>(request);
        if(requestTokenPtr != nullptr) {
            accessToken = requestTokenPtr->accessToken();
        }
        int ret = checkAccessible(it.second.accessible, accessToken);
        CHECK_ERROR(ret);

        ret = it.second.callback(request, responseArray);
        CHECK_ERROR(ret);

        Log::D(Log::TAG, "Response:");
        for(const auto& response: responseArray) {
            Log::D(Log::TAG, "  ->  %s", response->str().c_str());
        }
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
        Log::E(Log::TAG, "Invalid access token: %s.", accessToken.c_str());
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
