#include "OneDrive.hpp"

#include <ErrCode.hpp>
#include <ErrCode.hpp>
#include <HttpClient.hpp>
#include <Log.hpp>
#include <OneDrive.hpp>
#include <StdFileSystem.hpp>

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
int OneDrive::makeDir(const std::string& dirPath)
{
    Log::V(Log::Tag::CD, "%s", FORMAT_METHOD);

    auto path = std::filesystem::path(dirPath);
    auto parentPath = path.parent_path().string();
    auto dirName = path.filename().string();

    HttpClient httpClient;

    std::stringstream url;
    url << driveUrl << "/" << RootDir;
    if(parentPath.empty() == false) {
        url << "/" << parentPath;
    }
    url << ":/children";
    int ret = httpClient.url(url.str());
    CHECK_ERROR(ret);

    ret = httpClient.setConnectTimeout(10000);
    CHECK_ERROR(ret);

    ret = httpClient.setHeader("Content-Type", "application/json");
    CHECK_ERROR(ret);

    ret = httpClient.setHeader("Authorization", "Bearer " + accessToken);
    CHECK_ERROR(ret);

    auto reqBody = std::make_shared<std::stringstream>();
    *reqBody << "{\"name\":\"" << dirName << "\",\"folder\":{}}";
    fprintf(stderr, "token=%s\n", accessToken.c_str());
    Log::E("ssssss", "body=%s", reqBody->str().c_str());

    ret = httpClient.syncPost(reqBody);
    CHECK_ERROR(ret);

    auto respStatus = httpClient.getResponseStatus();
    if(respStatus != 200 && respStatus != 201) {
        std::string respMsg;
        // std::ignore = httpClient.getResponseReason(respMsg);
        Log::W(Log::Tag::CD, "Failed to make dir [%s], http response is (%d)%s", dirName.c_str(), respStatus, respMsg.c_str());
        CHECK_ERROR(ErrCode::CloudDriveMakeDirFailed);
    }

    return 0;
}

int OneDrive::createFile(const std::string& filePath,
                         std::shared_ptr<std::istream> body)
{
    Log::V(Log::Tag::CD, "%s", FORMAT_METHOD);

    // HttpClient httpClient;

    // int ret = httpClient.url(driveUrl);
    // CHECK_ERROR(ret);

    // ret = httpClient.setConnectTimeout(10000);
    // CHECK_ERROR(ret);

    // ret = httpClient.setHeader("Content-Type", "application/json");
    // CHECK_ERROR(ret);

    // ret = httpClient.setHeader("Authorization", "Bearer " + accessToken);
    // CHECK_ERROR(ret);

    // auto reqBody = std::make_shared<std::stringstream>();
    // *reqBody << "{\"name\":\"" << dirName << "\",\"folder\":{}}";

    // ret = httpClient.syncPost(reqBody);
    // CHECK_ERROR(ret);

    // auto respStatus = httpClient.getResponseStatus();
    // if(respStatus != 200 && respStatus != 201) {
    //     std::string respMsg;
    //     std::ignore = httpClient.getResponseReason(respMsg);
    //     Log::W(Log::Tag::CD, "Failed to make dir [%s], http response is (%d)%s", dirName.c_str(), respStatus, respMsg.c_str())
    //     CHECK_ERROR(ErrCode::CloudDriveMakeDirFailed);
    // }

    return 0;
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
OneDrive::OneDrive(const std::string& driveUrl, const std::string& accessToken)
    : driveUrl(driveUrl)
    , accessToken(accessToken)
{
}

OneDrive::~OneDrive()
{
}

} // namespace trinity


