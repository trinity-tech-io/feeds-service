#include "OneDrive.hpp"

#include <ErrCode.hpp>
#include <HttpClient.hpp>
#include <Log.hpp>
#include <OneDrive.hpp>

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
int OneDrive::makeDir(const std::string& dirName)
{
    HttpClient httpClient;

    int ret = httpClient.url(driveUrl);
    CHECK_ERROR(ret);

    ret = httpClient.setConnectTimeout(10000);
    CHECK_ERROR(ret);

    ret = httpClient.setHeader("Content-Type", "application/json");
    CHECK_ERROR(ret);

    ret = httpClient.setHeader("Authorization", "Bearer " + accessToken);
    CHECK_ERROR(ret);

    auto reqBody = std::make_shared<std::stringstream>();
    *reqBody << "{\"name\":\"FeedsServiceBackup\",\"folder\":{}}";

    ret = httpClient.syncPost(reqBody);
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

} // namespace trinity


