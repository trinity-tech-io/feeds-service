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

    int ret = 0;
    request = MakeRequest(method);
    if(request == nullptr) {
        request = std::shared_ptr<Request>();
        ret = ErrCode::UnimplementedError;
    }
    mpRoot.convert(*request);

    return ret;
}

int Factory::Marshal(const std::shared_ptr<Response>& response, std::vector<uint8_t>& data)
{
    msgpack::sbuffer mpBuf;
    msgpack::pack(mpBuf, *response);

    auto mpBufPtr = reinterpret_cast<uint8_t*>(mpBuf.data());
    data = {mpBufPtr, mpBufPtr + mpBuf.size()};

    return 0;
}

std::shared_ptr<Request> Factory::MakeRequest(const std::string& method)
{
    std::shared_ptr<Request> request;

    if(method == Method::GetMultiComments) {
        request = std::make_shared<GetMultiCommentsRequest>();
    } else {
        Log::E(Log::TAG, "Failed to make request from method: %s.", method.c_str());
    }

    return request;
}

std::shared_ptr<Response> Factory::MakeResponse(const std::string& method)
{
    std::shared_ptr<Response> response;

    if(method == Method::GetMultiComments) {
        response = std::make_shared<GetMultiCommentsResponse>();
    } else {
        Log::E(Log::TAG, "Failed to make response from method: %s.", method.c_str());
    }

    return response;
}

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */
// std::shared_ptr<msgpack_unpacked> Request::makeUnpackedObject()
// {
//     // auto creater = [&]() -> msgpack_unpacked* {
//     //     msgpack_unpacked* ptr = new msgpack_unpacked;
//     //     if(ptr != nullptr) {
//     //         msgpack_unpacked_init(ptr);
//     //     }
//     //     return ptr;
//     // };
//     // auto deleter = [](msgpack_unpacked* ptr) -> void {
//     //     if(ptr != nullptr) {
//     //         msgpack_unpacked_destroy(ptr);
//     //         delete ptr;
//     //     }
//     // };

//     // auto ptr = std::shared_ptr<msgpack_unpacked>(creater(), deleter);
//     // return ptr;
// }



/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */

} // namespace Rpc
} // namespace trinity
