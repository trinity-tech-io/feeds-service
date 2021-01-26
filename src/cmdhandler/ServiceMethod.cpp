#include "ServiceMethod.hpp"

#include <CloudDrive.hpp>
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
ServiceMethod::ServiceMethod(const std::filesystem::path& cacheDir)
    : cacheDir(cacheDir)
{
    using namespace std::placeholders;
    std::map<const char*, AdvancedHandler> advancedHandlerMap {
        {
            Rpc::Factory::Method::BackupServiceData,
            {std::bind(&ServiceMethod::onBackupServiceData, this, _1, _2, _3), Accessible::Owner}
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
int ServiceMethod::onBackupServiceData(const std::string& from,
                                       std::shared_ptr<Rpc::Request> request,
                                       std::vector<std::shared_ptr<Rpc::Response>>& responseArray)
{
    auto requestPtr = std::dynamic_pointer_cast<Rpc::BackupServiceDataRequest>(request);
    CHECK_ASSERT(requestPtr != nullptr, ErrCode::InvalidArgument);
    const auto& params = requestPtr->params;
    responseArray.clear();

    const auto badType = static_cast<CloudDrive::Type>(-1);
    auto type = badType;
    if(params.drive_name == "OneDrive") {
        type = CloudDrive::Type::OneDrive;
    }
    CHECK_ASSERT(type != badType, ErrCode::InvalidParams);
    
    auto drive = CloudDrive::Create(type, params.drive_url, params.drive_access_token);
    CHECK_ASSERT(drive != nullptr, ErrCode::InvalidParams);

    int ret = drive->makeDir("yyyyy/zzzz/testttttt");
    CHECK_ERROR(ret);

    return 0;
}

} // namespace trinity