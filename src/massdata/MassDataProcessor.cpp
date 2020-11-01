#include "MassDataProcessor.hpp"

#include <cstring>
#include <ela_carrier.h>
#include <ela_session.h>
#include <functional>
#include <SafePtr.hpp>

#include <crystal.h>
extern "C" {
#define new fix_cpp_keyword_new
#include <auth.h>
#include <did.h>
#undef new
}

#if defined(__APPLE__)
namespace std {
template <class T, class U>
static std::shared_ptr<T> reinterpret_pointer_cast(const std::shared_ptr<U> &r) noexcept
{
    auto p = reinterpret_cast<typename std::shared_ptr<T>::element_type *>(r.get());
    return std::shared_ptr<T>(r, p);
}
} // namespace std
#endif // defined(__APPLE__)

namespace trinity {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */
MassDataProcessor::MassDataProcessor()
{
    using namespace std::placeholders;
    mothodHandleMap = {
        {Method::SetBinary, std::bind(&MassDataProcessor::onSetBinary, this, _1, _2, _3)},
        {Method::GetBinary, std::bind(&MassDataProcessor::onGetBinary, this, _1, _2, _3)},
    };
}

MassDataProcessor::~MassDataProcessor()
{
}

void MassDataProcessor::config(const std::filesystem::path& massDataDir)
{
    this->massDataDir = massDataDir;
}

int MassDataProcessor::dispose(const std::vector<uint8_t>& headData,
                               const std::filesystem::path& bodyPath)
{
    Log::V(Log::TAG, "%s", __PRETTY_FUNCTION__);

    auto deleter = [](void* ptr) -> void {
        deref(ptr);
    };

    Req *reqBuf = nullptr;
    int ret = rpc_unmarshal_req(headData.data(), headData.size(), &reqBuf);
    auto req = std::shared_ptr<Req>(reqBuf, deleter); // workaround: declare for auto release Req pointer
    auto resp = std::make_shared<Resp>();
    if (ret >= 0) {
        Log::D(Log::TAG, "Mass data processor: dispose method [%s]", req->method);
        ret = ErrCode::UnimplementedError;
        for (const auto& it : mothodHandleMap) {
            if (std::strcmp(it.first, req->method) == 0) {
                ret = it.second(req, bodyPath, resp);
                break;
            }
        }
    } else if (ret == -1) {
        ret = ErrCode::MassDataUnmarshalReqFailed;
    } else if (ret == -2) {
        ret = ErrCode::MassDataUnknownReqFailed;
    } else if (ret == -3) {
        ret = ErrCode::MassDataUnsupportedVersion;
    } else {
        ret = ErrCode::UnknownError;
    }
    if(ret < ErrCode::StdSystemErrorIndex) {
        ret = ErrCode::StdSystemError;
    }

    Marshalled* marshalBuf = nullptr;
    if(ret >= 0) {
        marshalBuf = rpc_marshal_resp(req->method, resp.get());
    } else {
        CHECK_ASSERT(req, ret);
        auto errDesp = ErrCode::ToString(ret);
        marshalBuf = rpc_marshal_err(req->tsx_id, ret, errDesp.c_str());
        Log::D(Log::TAG, "Response error:");
        Log::D(Log::TAG, "    code: %d", ret);
        Log::D(Log::TAG, "    message: %s", errDesp.c_str());
    }
    auto marshalData = std::shared_ptr<Marshalled>(marshalBuf, deleter); // workaround: declare for auto release Marshalled pointer
    CHECK_ASSERT(marshalData, ErrCode::MassDataMarshalRespFailed);

    auto marshalDataPtr = reinterpret_cast<uint8_t*>(marshalData->data);
    resultHeadData = {marshalDataPtr, marshalDataPtr + marshalData->sz};

    return 0;
}

int MassDataProcessor::getResultAndReset(std::vector<uint8_t>& headData,
                                         std::filesystem::path& bodyPath)
{
    headData = std::move(resultHeadData);
    bodyPath = std::move(resultBodyPath);

    resultHeadData.clear();
    resultBodyPath.clear();

    return 0;
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
int MassDataProcessor::isOwner(const std::string& accessToken)
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

int MassDataProcessor::isMember(const std::string& accessToken)
{
    std::shared_ptr<UserInfo> userInfo;
    int ret = getUserInfo(accessToken, userInfo);
    CHECK_ERROR(ret);

    return 0;
}

int MassDataProcessor::getUserInfo(const std::string& accessToken, std::shared_ptr<UserInfo>& userInfo)
{
    if (did_is_ready() == false) {
        Log::E(Log::TAG, "Mass data processor: Feeds DID is not ready.");
        CHECK_ERROR(ErrCode::DidNotReady);
    }

    auto creater = [&]() -> UserInfo* {
        Log::I(Log::TAG, "Create user info.");
        auto ptr = create_uinfo_from_access_token(accessToken.c_str());
        return ptr;
    };
    auto deleter = [=](UserInfo* ptr) -> void {
        Log::I(Log::TAG, "Destroy user info.");
        deref(ptr);
    };
    userInfo = std::shared_ptr<UserInfo>(creater(), deleter);
    if (userInfo == nullptr) {
        Log::E(Log::TAG, "Mass data processor: Invalid access token.");
        CHECK_ERROR(ErrCode::InvalidAccessToken);
    }

    return 0;
}

int MassDataProcessor::onSetBinary(std::shared_ptr<Req> req,
                                   const std::filesystem::path& bodyPath,
                                   std::shared_ptr<Resp>& resp)
{
    auto setBinReq = std::reinterpret_pointer_cast<SetBinaryReq>(req);
    Log::D(Log::TAG, "Request params:");
    Log::D(Log::TAG, "    access_token: %s", setBinReq->params.tk);
    Log::D(Log::TAG, "    key: %s", setBinReq->params.key);
    Log::D(Log::TAG, "    algo: %s", setBinReq->params.algo);
    Log::D(Log::TAG, "    checksum: %s", setBinReq->params.checksum);

    auto setBinResp = std::make_shared<SetBinaryResp>();
    setBinResp->tsx_id = setBinReq->tsx_id;

    // TODO: Comment For Test
    // int ret = isOwner(setBinReq->params.tk);
    // CHECK_ERROR(ret);

    if(std::strcmp(setBinReq->params.algo, "None") != 0) {
        CHECK_ERROR(ErrCode::MassDataUnsupportedAlgo);
    }

    auto keyPath = massDataDir / setBinReq->params.key;
    Log::V(Log::TAG, "Resave %s to %s.", bodyPath.c_str(), keyPath.c_str());
    std::error_code ec;
    std::filesystem::rename(bodyPath, keyPath, ec); // noexcept
    if(ec.value() != 0) {
        CHECK_ERROR(ErrCode::StdSystemErrorIndex + (-ec.value()));
    }

    setBinResp->result.key = setBinReq->params.key;
    Log::D(Log::TAG, "Response result:");
    Log::D(Log::TAG, "    key: %s", setBinResp->result.key);

    resp = std::reinterpret_pointer_cast<Resp>(setBinResp);

    return 0;
}

int MassDataProcessor::onGetBinary(std::shared_ptr<Req> req,
                                   const std::filesystem::path& bodyPath,
                                   std::shared_ptr<Resp>& resp)
{
    auto getBinReq = std::reinterpret_pointer_cast<GetBinaryReq>(req);
    Log::D(Log::TAG, "    access_token: %s", getBinReq->params.tk);
    Log::D(Log::TAG, "    key: %s", getBinReq->params.key);

    auto getBinResp = std::make_shared<GetBinaryResp>();
    getBinResp->tsx_id = getBinReq->tsx_id;

    // TODO: Comment For Test
    // int ret = isMember(getBinReq->params.tk);
    // CHECK_ERROR(ret);

    auto keyPath = massDataDir / getBinReq->params.key;
    Log::V(Log::TAG, "Try to load %s.", keyPath.c_str());
    if(std::filesystem::exists(keyPath) == false) {
        CHECK_ERROR(ErrCode::FileNotExistsError);
    }

    resultBodyPath = keyPath;

    getBinResp->result.key = getBinReq->params.key;
    getBinResp->result.algo = const_cast<char*>("None");
    getBinResp->result.checksum = const_cast<char*>("");
    Log::D(Log::TAG, "Response result:");
    Log::D(Log::TAG, "    key: %s", getBinResp->result.key);
    Log::D(Log::TAG, "    algo: %s", getBinResp->result.algo);
    Log::D(Log::TAG, "    checksum: %s", getBinResp->result.checksum);

    resp = std::reinterpret_pointer_cast<Resp>(getBinResp);

    return 0;
}

} // namespace trinity