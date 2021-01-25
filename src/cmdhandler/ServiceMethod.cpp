#include "ServiceMethod.hpp"

#include <AutoUpdate.hpp>
#include <CloudDrive.hpp>
#include <ErrCode.hpp>
#include <Log.hpp>
#include <Platform.hpp>

#undef FEAT_AUTOUPDATE

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
ServiceMethod::ServiceMethod(const std::filesystem::path& cacheDir,
                             const std::vector<const char*>& execArgv)
    : cacheDir(cacheDir)
    , execArgv(execArgv)
{
    using namespace std::placeholders;
    std::map<const char*, AdvancedHandler> advancedHandlerMap {
#ifdef FEAT_AUTOUPDATE
        {
            Rpc::Factory::Method::DownloadNewService,
            {std::bind(&ServiceMethod::onDownloadNewService, this, _1, _2, _3), Accessible::Owner}
        },
        {
            Rpc::Factory::Method::StartNewService,
            {std::bind(&ServiceMethod::onStartNewService, this, _1, _2, _3), Accessible::Owner}
        },
#endif // FEAT_AUTOUPDATE
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
int ServiceMethod::onDownloadNewService(const std::string &from,
                                        std::shared_ptr<Rpc::Request> request,
                                        std::vector<std::shared_ptr<Rpc::Response>>& responseArray)
{
    auto requestPtr = std::dynamic_pointer_cast<Rpc::DownloadNewServiceRequest>(request);
    CHECK_ASSERT(requestPtr != nullptr, ErrCode::InvalidArgument);
    const auto& params = requestPtr->params;
    responseArray.clear();

    int needUpdate = AutoUpdate::GetInstance()->needUpdate(params.new_version);
    CHECK_ERROR(needUpdate);

    auto execAbsPath = std::filesystem::absolute(execArgv[0]);
    auto runtimePath = execAbsPath.parent_path().parent_path().parent_path(); // remove [current/bin/feedsd]
    auto dirExists = std::filesystem::exists(runtimePath);
    CHECK_ASSERT(dirExists, ErrCode::AutoUpdateBadRuntimeDir);

    const Rpc::DownloadNewServiceRequest::Params::Tarball* tarball = nullptr;
    auto osName = Platform::GetProductName();
    if(osName == "macosx") {
        tarball = &params.macosx;
    } else if(osName == "ubuntu") {
        auto osVer = Platform::GetProductVersion();
        if(osVer == "18.04") {
            tarball = &params.ubuntu_1804;
        } else if(osVer == "20.04") {
            tarball = &params.ubuntu_2004;
        }
    } else if(osName == "raspbian") {
        tarball = &params.raspbian;
    }
    Log::D(Log::Tag::Cmd, "Updating feeds service to %s on %s", params.new_version.c_str(), osName.c_str());
    CHECK_ASSERT(tarball != nullptr, ErrCode::AutoUpdateUnsuppertProduct);
    std::string tarballUrl = params.base_url + "/" + tarball->name;

    auto resultCallback = [this, to = from, requestPtr](int errCode) {
        Log::I(Log::Tag::Cmd, "Download new service return %d", errCode);
        int ret;
        if(errCode < 0) {
            auto error = Rpc::Factory::MakeError(errCode);
            error->id = requestPtr->id;
            ret = this->error(to, error);
        } else {
            auto content = Rpc::Factory::MakeNotify(requestPtr->method);
            ret = this->notify(Accessible::Owner, content);
        }
        CHECK_RETVAL(ret);
    };
    int ret = AutoUpdate::GetInstance()->asyncDownloadTarball(params.new_version, tarballUrl, runtimePath, cacheDir,
                                                              tarball->name, tarball->size, tarball->md5,
                                                              resultCallback);
    CHECK_ERROR(ret);

    // push last response or empty response
    auto responsePtr = Rpc::Factory::MakeResponse(request->method);
    auto response = std::dynamic_pointer_cast<Rpc::DownloadNewServiceResponse>(responsePtr);
    response->version = request->version;
    response->id = request->id;
    responseArray.push_back(response);

    return 0;
}

int ServiceMethod::onStartNewService(const std::string& from,
                                     std::shared_ptr<Rpc::Request> request,
                                     std::vector<std::shared_ptr<Rpc::Response>>& responseArray)
{
    auto requestPtr = std::dynamic_pointer_cast<Rpc::StartNewServiceRequest>(request);
    CHECK_ASSERT(requestPtr != nullptr, ErrCode::InvalidArgument);
    const auto& params = requestPtr->params;
    responseArray.clear();

    int needUpdate = AutoUpdate::GetInstance()->needUpdate(params.new_version);
    CHECK_ERROR(needUpdate);

    auto execAbsPath = std::filesystem::absolute(execArgv[0]);
    auto runtimePath = execAbsPath.parent_path().parent_path().parent_path(); // remove [current/bin/feedsd]
    auto dirExists = std::filesystem::exists(runtimePath);
    CHECK_ASSERT(dirExists, ErrCode::AutoUpdateBadRuntimeDir);

    std::stringstream launchCmd;
    for(const auto& it: execArgv) {
        launchCmd << "'" << it << "' ";
    }
    int ret = AutoUpdate::GetInstance()->startTarball(runtimePath, cacheDir, params.new_version, launchCmd.str());
    CHECK_ERROR(ret);

    // push last response or empty response
    auto responsePtr = Rpc::Factory::MakeResponse(request->method);
    auto response = std::dynamic_pointer_cast<Rpc::StartNewServiceResponse>(responsePtr);
    response->version = request->version;
    response->id = request->id;
    responseArray.push_back(response);

    return 0;
}

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

    int ret = drive->makeDir(MigrateDir);
    CHECK_ERROR(ret);

    return 0;
}

} // namespace trinity