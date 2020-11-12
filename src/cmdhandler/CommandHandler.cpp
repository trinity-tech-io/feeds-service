#include "CommandHandler.hpp"

#include <SafePtr.hpp>
#include <MassData.hpp>

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
    carrierHandler = carrier;

    cmdListener = std::move(std::vector<std::shared_ptr<Listener>> {
        std::make_shared<MassData>(dataDir / MassData::MassDataDirName),

    });

    return 0;
}

void CommandHandler::cleanup()
{
    CmdHandlerInstance.reset();

    carrierHandler.reset();
    cmdListener.clear();
}

int CommandHandler::process(const char* from, const std::vector<uint8_t>& data)
{
    Req *reqBuf = nullptr;
    int ret = rpc_unmarshal_req(data.data(), data.size(), &reqBuf);
    auto deleter = [](void* ptr) -> void {
        deref(ptr);
    };
    auto req = std::shared_ptr<Req>(reqBuf, deleter); // workaround: declare for auto release Req pointer
    auto resp = std::make_shared<Resp>();
    if (ret < 0) {
        Log::E(Log::TAG, "Failed to  unmarshal req method:[%s]", req ? req->method : "unknown");
        if (ret == -1) {
            ret = ErrCode::MassDataUnmarshalReqFailed;
        } else if (ret == -2) {
            ret = ErrCode::MassDataUnknownReqFailed;
        } else if (ret == -3) {
            ret = ErrCode::MassDataUnsupportedVersion;
        } else {
            ret = ErrCode::UnknownError;
        }

        ErrResp errResp = {
            .tsx_id = req ? req->tsx_id : -1,
            .ec     = ret 
        };
        Marshalled *respMarshal = rpc_marshal_err_resp(&errResp);
        msgq_enq(from, respMarshal);
        deref(respMarshal);
        return ret;
    }

    Log::D(Log::TAG, "Dispose method [%s]", req->method);
    ret = ErrCode::UnimplementedError;
    for (const auto &it : cmdListener) {
        ret = it->onDispose(req, resp);
        if (ret != ErrCode::UnimplementedError) {
            break;
        }
    }
    if(ret == ErrCode::UnimplementedError) { // return if unprocessed.
        return ErrCode::UnimplementedError;
    }
    if(ret < 0) {
        if (ret < ErrCode::StdSystemErrorIndex) {
            ret = ErrCode::StdSystemError;
        }

        ErrResp errResp = {
            .tsx_id = req ? req->tsx_id : -1,
            .ec     = ret 
        };
        Marshalled *respMarshal = rpc_marshal_err_resp(&errResp);
        msgq_enq(from, respMarshal);
        deref(respMarshal);
        return ret;
    }

    assert(resp.get() != nullptr);
    Marshalled *respMarshal = rpc_marshal_resp(req->method, resp.get());
    msgq_enq(from, respMarshal);
    deref(respMarshal);

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

int CommandHandler::Listener::onDispose(std::shared_ptr<Req> req, std::shared_ptr<Resp> &resp)
{
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