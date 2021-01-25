#include "RpcFactory.hpp"

#include <ErrCode.hpp>
#include <Log.hpp>

extern "C" {
#include <db.h>
}


namespace trinity {
namespace Rpc {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */
int Factory::Unmarshal(const std::vector<uint8_t>& data, std::shared_ptr<Rpc::Base>& rpc)
{
    msgpack::object_handle mpUnpackHandle;
    try {
        mpUnpackHandle = msgpack::unpack(reinterpret_cast<const char*>(data.data()), data.size());
    } catch(...) {
        CHECK_ASSERT(false, ErrCode::MsgPackParseFailed);
    }
    const msgpack::object& mpRoot = mpUnpackHandle.get();
    CHECK_ASSERT(mpRoot.type == msgpack::type::MAP, ErrCode::MsgPackInvalidStruct);
    auto root = mpRoot.as<Dict>();

    const auto& methodIt = root.find(DictKeyMethod);
    CHECK_ASSERT(methodIt != root.end(), ErrCode::MsgPackInvalidStruct);
    CHECK_ASSERT(methodIt->second.type == msgpack::type::STR, ErrCode::MsgPackInvalidValue);
    auto method = methodIt->second.as<std::string>();
    CHECK_ASSERT(method.empty() == false, ErrCode::MsgPackInvalidValue);

    int processed = 0;
    auto request = MakeRequest(method);
    if(request == nullptr) {
        request = std::make_shared<Request>();
        processed = ErrCode::UnimplementedError;
    }
    try {
        request->unpack(mpRoot);
    } catch(...) {
        CHECK_ASSERT(false, ErrCode::MsgPackParseFailed);
    }
    CHECK_ASSERT(request->method.empty() == false, ErrCode::MsgPackParseFailed);
    rpc = request;

    return processed;
}

int Factory::Marshal(const std::shared_ptr<Rpc::Base>& rpc, std::vector<uint8_t>& data)
{
    msgpack::sbuffer mpBuf;

    rpc->pack(mpBuf);

    auto mpBufPtr = reinterpret_cast<uint8_t*>(mpBuf.data());
    data = {mpBufPtr, mpBufPtr + mpBuf.size()};

    return data.size();
}

std::shared_ptr<Request> Factory::MakeRequest(const std::string& method)
{
    std::shared_ptr<Request> request;

    if(method == Method::StandardSignIn) {
        request = std::make_shared<StandardSignInRequest>();
    } else if(method == Method::StandardDidAuth) {
        request = std::make_shared<StandardDidAuthRequest>();
    } else if(method == Method::GetMultiComments) {
        request = std::make_shared<GetMultiCommentsRequest>();
    } else if(method == Method::GetMultiLikesAndCommentsCount) {
        request = std::make_shared<GetMultiLikesAndCommentsCountRequest>();
    } else if(method == Method::GetMultiSubscribersCount) {
        request = std::make_shared<GetMultiSubscribersCountRequest>();
    } else if(method == Method::BackupServiceData) {
        request = std::make_shared<BackupServiceDataRequest>();
    }

    if(request != nullptr) {
        request->version = "1.0";
        request->method = method;
    }

    return request;
}

std::shared_ptr<Response> Factory::MakeResponse(const std::string& method)
{
    std::shared_ptr<Response> response;

    if(method == Method::StandardSignIn) {
        response = std::make_shared<StandardSignInResponse>();
    } else if(method == Method::StandardDidAuth) {
        response = std::make_shared<StandardDidAuthResponse>();
    } else if(method == Method::GetMultiComments) {
        response = std::make_shared<GetMultiCommentsResponse>();
    } else if(method == Method::GetMultiLikesAndCommentsCount) {
        response = std::make_shared<GetMultiLikesAndCommentsCountResponse>();
    } else if(method == Method::GetMultiSubscribersCount) {
        response = std::make_shared<GetMultiSubscribersCountResponse>();
    } else if(method == Method::BackupServiceData) {
        response = std::make_shared<BackupServiceDataResponse>();
    } else {
        Log::E(Log::Tag::Rpc, "RPC Factory ignore to make response from method: %s.", method.c_str());
    }

    response->version = "1.0";

    return response;
}

std::shared_ptr<Notify> Factory::MakeNotify(const std::string& method)
{
    std::shared_ptr<Notify> notify;

    if(method == Method::BackupServiceData) {
        notify = std::make_shared<BackupServiceDataNotify>();
    } else {
        Log::E(Log::Tag::Rpc, "RPC Factory ignore to make notify from method: %s.", method.c_str());
    }

    notify->version = "1.0";
    notify->method = method;

    return notify;
}

std::shared_ptr<Error> Factory::MakeError(int errCode)
{
    std::shared_ptr<Error> error = std::make_shared<Error>();

    error->version = "1.0";

    error->error.code = errCode;
    error->error.message = ErrCode::ToString(errCode);

    return error;
}

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */

} // namespace Rpc
} // namespace trinity
