#include "OneDrive.hpp"

#include <regex>
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

    std::stringstream url;
    url << driveUrl << "/" << RootDir;
    if(parentPath.empty() == false) {
        url << "/" << parentPath;
    }
    url << ":/children";

    HttpClient httpClient;
    int ret = makeHttpClient(url.str(), httpClient);
    CHECK_ERROR(ret);

    ret = httpClient.setHeader("content-type", "application/json");
    CHECK_ERROR(ret);

    auto respBody = std::make_shared<std::stringstream>();
    ret = httpClient.setResponseBody(respBody);
    CHECK_ERROR(ret);

    auto reqBody = std::make_shared<std::stringstream>();
    *reqBody << "{"
             << "  \"name\":\"" << dirName << "\","
             << "  \"folder\":{}"
             << "}";
    ret = httpClient.syncPost(reqBody);
    CHECK_ERROR(ret);

    auto respStatus = httpClient.getResponseStatus();
    if(respStatus != 200 && respStatus != 201) {
        Log::W(Log::Tag::CD, "Failed to make dir [%s], http response is (%d)%s", dirPath.c_str(), respStatus, respBody->str().c_str());
        CHECK_ERROR(ErrCode::CloudDriveMakeDirFailed);
    }

    return 0;
}

int OneDrive::write(const std::string& filePath, std::shared_ptr<std::istream> content)
{
    Log::V(Log::Tag::CD, "%s", FORMAT_METHOD);

    std::string fileUrl;
    int ret = touchFile(filePath, fileUrl);
    CHECK_ERROR(ret);

    ret = uploadFile(fileUrl, content);
    CHECK_ERROR(ret);

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

int OneDrive::makeHttpClient(const std::string& url, HttpClient& httpClient)
{
    int ret = httpClient.url(url);
    CHECK_ERROR(ret);

    ret = httpClient.setConnectTimeout(10000);
    CHECK_ERROR(ret);

    ret = httpClient.setHeader("Authorization", "Bearer " + accessToken);
    CHECK_ERROR(ret);

    return 0;
}

int OneDrive::touchFile(const std::string& filePath, std::string& fileUrl)
{
    Log::V(Log::Tag::CD, "%s", FORMAT_METHOD);

    std::stringstream url;
    url << driveUrl << "/" << RootDir << "/" << filePath << ":/createUploadSession";

    HttpClient httpClient;
    int ret = makeHttpClient(url.str(), httpClient);
    CHECK_ERROR(ret);

    ret = httpClient.setHeader("content-type", "application/json");
    CHECK_ERROR(ret);

    auto respBody = std::make_shared<std::stringstream>();
    ret = httpClient.setResponseBody(respBody);
    CHECK_ERROR(ret);

    auto reqBody = std::make_shared<std::stringstream>();
    *reqBody << "{"
             << "  \"item\": {"
             << "    \"@microsoft.graph.conflictBehavior\": \"replace\""
             << "  }"
             << "}";
    ret = httpClient.syncPost(reqBody);
    CHECK_ERROR(ret);

    auto respStatus = httpClient.getResponseStatus();
    if(respStatus != 200 && respStatus != 201) {
        fprintf(stderr, "token=%s\n", accessToken.c_str());
        Log::W(Log::Tag::CD, "Failed to create file [%s], http response is (%d)%s", filePath.c_str(), respStatus, respBody->str().c_str());
        CHECK_ERROR(ErrCode::CloudDriveTouchFileFailed);
    }

    fileUrl = std::regex_replace(respBody->str(), std::regex(".*\"uploadUrl\":"), "");
    fileUrl = std::regex_replace(fileUrl, std::regex("\\}|\""), "");
    Log::D(Log::Tag::CD, "OneDrive createUploadSession: upload url=%s", fileUrl.c_str());

    return 0;
}

int OneDrive::uploadFile(const std::string& fileUrl, std::shared_ptr<std::istream> content)
{
    Log::V(Log::Tag::CD, "%s", FORMAT_METHOD);

    HttpClient httpClient;
    int ret = makeHttpClient(fileUrl, httpClient);
    CHECK_ERROR(ret);

    std::stringstream range;
    content->seekg(0, std::ios::end);
    int contentSize = content->tellg();
    range << "bytes 0-" << contentSize - 1 << "/" << contentSize;
    ret = httpClient.setHeader("content-range", range.str());
    CHECK_ERROR(ret);
    ret = httpClient.setHeader("content-type", "application/octet-stream");
    CHECK_ERROR(ret);

    auto respBody = std::make_shared<std::stringstream>();
    ret = httpClient.setResponseBody(respBody);
    CHECK_ERROR(ret);

    ret = httpClient.syncDo(HttpClient::Method::PUT, content);
    CHECK_ERROR(ret);

    auto respStatus = httpClient.getResponseStatus();
    if(respStatus != 200 && respStatus != 201) {
        fprintf(stderr, "token=%s\n", accessToken.c_str());
        Log::W(Log::Tag::CD, "Failed upload to [%s]", fileUrl.c_str());
        Log::W(Log::Tag::CD, "Http Response=(%d)%s", respStatus, respBody->str().c_str());
        CHECK_ERROR(ErrCode::CloudDriveUploadFileFailed);
    }

    return 0;
}

} // namespace trinity


