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
int Factory::Unmarshal(const std::vector<uint8_t>& data, std::shared_ptr<Request>& request)
{
    auto mpUnpackHandle = msgpack::unpack(reinterpret_cast<const char*>(data.data()), data.size());
    const msgpack::object& mpRoot = mpUnpackHandle.get();
    CHECK_ASSERT(mpRoot.type == msgpack::type::MAP, ErrCode::MsgPackInvalidStruct);
    auto root = mpRoot.as<Dict>();

    const auto& methodIt = root.find(DictKeyMethod);
    CHECK_ASSERT(methodIt != root.end(), ErrCode::MsgPackInvalidStruct);
    CHECK_ASSERT(methodIt->second.type == msgpack::type::STR, ErrCode::MsgPackInvalidValue);
    auto method = methodIt->second.as<std::string>();
    CHECK_ASSERT(method.empty() == false, ErrCode::MsgPackInvalidValue);

    int processed = 0;
    request = MakeRequest(method);
    if(request == nullptr) {
        request = std::make_shared<Request>();
        processed = ErrCode::UnimplementedError;
    }
    request->unpack(mpRoot);
    CHECK_ASSERT(request->method.empty() == false, ErrCode::MsgPackParseFailed);

    return processed;
}

int Factory::Marshal(const std::shared_ptr<Response>& response, std::vector<uint8_t>& data)
{
    msgpack::sbuffer mpBuf;

    response->pack(mpBuf);

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
    } else {
        Log::E(Log::Tag::Rpc, "RPC Factory ignore to make response from method: %s.", method.c_str());
    }

    return response;
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
