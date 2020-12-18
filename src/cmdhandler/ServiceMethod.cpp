#include "ServiceMethod.hpp"

#include <AutoUpdate.hpp>
#include <ErrCode.hpp>
#include <Log.hpp>

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
ServiceMethod::ServiceMethod(const std::filesystem::path& cacheDir)
    : cacheDir(cacheDir)
{
    using namespace std::placeholders;
    std::map<const char*, AdvancedHandler> advancedHandlerMap {
        {
            Rpc::Factory::Method::AutoUpdateService,
            {std::bind(&ServiceMethod::onAutoUpdateService, this, _1, _2), Accessible::Owner}
        },
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
int ServiceMethod::onAutoUpdateService(std::shared_ptr<Rpc::Request> request,
                                       std::vector<std::shared_ptr<Rpc::Response>>& responseArray)
{
    auto requestPtr = std::dynamic_pointer_cast<Rpc::AutoUpdateServiceRequest>(request);
    CHECK_ASSERT(requestPtr != nullptr, ErrCode::InvalidArgument);
    const auto& params = requestPtr->params;
    responseArray.clear();

    bool validArgus = ( params.access_token.empty() == false);
    CHECK_ASSERT(validArgus, ErrCode::InvalidArgument);

    // push last response or empty response
    auto responsePtr = Rpc::Factory::MakeResponse(request->method);
    auto response = std::dynamic_pointer_cast<Rpc::AutoUpdateServiceResponse>(responsePtr);
    if(response != nullptr) {
        response->version = request->version;
        response->id = request->id;
    }
    responseArray.push_back(response);

    return -1;
}

} // namespace trinity