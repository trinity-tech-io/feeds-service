#include "ServiceMethod.hpp"

#include <ver.h>
#include <ErrCode.hpp>
#include <Log.hpp>
#include <Platform.hpp>

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
ServiceMethod::ServiceMethod(const std::filesystem::path& dataDir,
                             const std::filesystem::path& cacheDir)
    : dataDir(dataDir)
    , cacheDir(cacheDir)
{
    using namespace std::placeholders;
    std::map<const char*, AdvancedHandler> advancedHandlerMap {
        {
            Rpc::Factory::Method::GetServiceVersion,
            {std::bind(&ServiceMethod::onGetServiceVersion, this, _1, _2), Accessible::Anyone}
        }
    };

    setHandleMap({}, advancedHandlerMap);
}

ServiceMethod::~ServiceMethod()
{
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
int ServiceMethod::onGetServiceVersion(std::shared_ptr<Rpc::Request> request,
                                       std::vector<std::shared_ptr<Rpc::Response>>& responseArray)
{
    auto requestPtr = std::dynamic_pointer_cast<Rpc::GetServiceVersionRequest>(request);
    CHECK_ASSERT(requestPtr != nullptr, ErrCode::InvalidArgument);
    responseArray.clear();

    auto responsePtr = Rpc::Factory::MakeResponse(request->method);
    auto response = std::dynamic_pointer_cast<Rpc::GetServiceVersionResponse>(responsePtr);
    CHECK_ASSERT(response != nullptr, ErrCode::RpcUnimplementedError);
    response->version = request->version;
    response->id = request->id;
    response->result.version = FEEDSD_VER;
    response->result.version_code = FEEDSD_VERCODE;

    responseArray.push_back(response);

    return 0;
}


} // namespace trinity