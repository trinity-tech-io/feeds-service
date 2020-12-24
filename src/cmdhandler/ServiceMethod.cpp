#include "ServiceMethod.hpp"

#include <AutoUpdate.hpp>
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
ServiceMethod::ServiceMethod(const std::filesystem::path &cacheDir,
                             const std::filesystem::path &execPath)
    : cacheDir(cacheDir)
    , execPath(execPath)
{
    using namespace std::placeholders;
    std::map<const char*, AdvancedHandler> advancedHandlerMap {
        {
            Rpc::Factory::Method::DownloadNewService,
            {std::bind(&ServiceMethod::onDownloadNewService, this, _1, _2), Accessible::Owner}
        },
        {
            Rpc::Factory::Method::StartNewService,
            {std::bind(&ServiceMethod::onStartNewService, this, _1, _2), Accessible::Owner}
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
int ServiceMethod::onDownloadNewService(std::shared_ptr<Rpc::Request> request,
                                        std::vector<std::shared_ptr<Rpc::Response>>& responseArray)
{
    auto requestPtr = std::dynamic_pointer_cast<Rpc::DownloadNewServiceRequest>(request);
    CHECK_ASSERT(requestPtr != nullptr, ErrCode::InvalidArgument);
    const auto& params = requestPtr->params;
    responseArray.clear();

    bool validArgus = ( params.access_token.empty() == false);
    CHECK_ASSERT(validArgus, ErrCode::InvalidArgument);

    int needUpdate = AutoUpdate::GetInstance()->needUpdate(params.new_version_code);
    CHECK_ERROR(needUpdate);

    auto execAbsPath = std::filesystem::absolute(execPath);
    auto runtimePath = execAbsPath.parent_path().parent_path().parent_path(); // remove [current/bin/feedsd]
    auto dirExists = std::filesystem::exists(runtimePath);
    CHECK_ASSERT(dirExists, ErrCode::AutoUpdateBadRuntimeDir);

    const Rpc::DownloadNewServiceRequest::Params::Tarball* tarball = nullptr;
    auto osName = Platform::GetProductName();
    if(osName == "MacOSX") {
        tarball = &params.macosx;
    } else if(osName == "Ubuntu") {
        auto osVer = Platform::GetProductVersion();
        if(osVer == "18.04") {
            tarball = &params.ubuntu_1804;
        } else if(osVer == "20.04") {
            tarball = &params.ubuntu_2004;
        }
    } else if(osName == "RaspberryPi") { // TODO: RaspberryPi is wrong name
        tarball = &params.raspberrypi;
    }
    Log::D(Log::Tag::Cmd, "Updating feeds service to %lld on %s", params.new_version_code, osName.c_str());
    CHECK_ASSERT(tarball != nullptr, ErrCode::AutoUpdateUnsuppertProduct);
    std::string tarballUrl = params.base_url + "/" + tarball->name;

    auto resultCallback = [](int errCode) {

    };

    int ret = AutoUpdate::GetInstance()->asyncDownloadTarball(params.new_version_code, tarballUrl, runtimePath, cacheDir,
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

int ServiceMethod::onStartNewService(std::shared_ptr<Rpc::Request> request,
                                        std::vector<std::shared_ptr<Rpc::Response>>& responseArray)
{
    auto requestPtr = std::dynamic_pointer_cast<Rpc::StartNewServiceRequest>(request);
    CHECK_ASSERT(requestPtr != nullptr, ErrCode::InvalidArgument);
    const auto& params = requestPtr->params;
    responseArray.clear();

    bool validArgus = ( params.access_token.empty() == false);
    CHECK_ASSERT(validArgus, ErrCode::InvalidArgument);

    auto execAbsPath = std::filesystem::absolute(execPath);
    auto runtimePath = execAbsPath.parent_path().parent_path().parent_path(); // remove [current/bin/feedsd]
    auto dirExists = std::filesystem::exists(runtimePath);
    CHECK_ASSERT(dirExists, ErrCode::AutoUpdateBadRuntimeDir);

    int ret = AutoUpdate::GetInstance()->startTarball(runtimePath, params.new_version_code);
    CHECK_ERROR(ret);

    // push last response or empty response
    auto responsePtr = Rpc::Factory::MakeResponse(request->method);
    auto response = std::dynamic_pointer_cast<Rpc::StartNewServiceResponse>(responsePtr);
    response->version = request->version;
    response->id = request->id;
    responseArray.push_back(response);

    return 0;
}

} // namespace trinity