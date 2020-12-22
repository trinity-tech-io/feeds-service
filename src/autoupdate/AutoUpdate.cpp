#include "AutoUpdate.hpp"

#include <fstream>
#include <ver.h>
#include <ErrCode.hpp>
#include <HttpClient.hpp>
#include <MD5.hpp>
#include <Platform.hpp>
#include <ThreadPool.hpp>

#define CHECK_ERROR_WITHNOTIFY(errCode, callback) \
	if(errCode < 0) { \
		callback(errCode); \
		CHECK_ERROR(errCode); \
	}

namespace trinity {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */
std::shared_ptr<AutoUpdate> AutoUpdate::AutoUpdateInstance;

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */
std::shared_ptr<AutoUpdate> AutoUpdate::GetInstance()
{
    // ignore check thread-safty, no needed

    if(AutoUpdateInstance != nullptr) {
        return AutoUpdateInstance;
    }

    struct Impl: AutoUpdate {
    };
    AutoUpdateInstance = std::make_shared<Impl>();

    return AutoUpdateInstance;
}

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */
int AutoUpdate::needUpdate(int64_t newVerCode)
{
    int64_t currVerCode = FEEDSD_VERCODE;
    if(currVerCode >= newVerCode) {
        return ErrCode::AutoUpdateAlreadyNewest;
    }

    return 0;
}

int AutoUpdate::asyncDownloadTarball(const std::string& url,
                                     const std::filesystem::path& cacheDir,
                                     const std::string& name,
                                     int64_t size,
                                     const std::string& md5,
                                     const std::function<void(int errCode)>& resultCallback)
{
    if(threadPool.get() == nullptr) {
        threadPool = ThreadPool::Create("autoupdate");
    }

    threadPool->post([=] {
        downloadTarball(url, cacheDir, name, size, md5, resultCallback);
    });

    return 0;
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
int AutoUpdate::downloadTarball(const std::string& url,
                                const std::filesystem::path& cacheDir,
                                const std::string& name,
                                int64_t size,
                                const std::string& md5,
                                const std::function<void(int errCode)>& resultCallback)
{
    auto autoUpdateCacheDir = cacheDir / "autoupdate";
    auto autoUpdateFilePath = autoUpdateCacheDir / name;

    auto dirExists = std::filesystem::exists(autoUpdateCacheDir);
    if(dirExists == false) {
        dirExists = std::filesystem::create_directories(autoUpdateCacheDir);
    }
    CHECK_ASSERT(dirExists, ErrCode::FileNotExistsError);

    int ret = checkTarball(autoUpdateFilePath, size, md5);
    if(ret != 0) {
        Log::D(Log::Tag::AU, "Downloading update tarball from %s.", url.c_str());
        HttpClient httpClient;
        ret = httpClient.url(url);
        CHECK_ERROR_WITHNOTIFY(ret, resultCallback);

        auto tarballStream = std::make_shared<std::ofstream>(autoUpdateFilePath);
        ret = httpClient.syncGet(tarballStream);
        CHECK_ERROR_WITHNOTIFY(ret, resultCallback);
        tarballStream->flush();
        tarballStream->close();

        ret = checkTarball(autoUpdateFilePath, size, md5);
        CHECK_ERROR_WITHNOTIFY(ret, resultCallback);
    }

    Log::D(Log::Tag::AU, "Unpacking update tarball from %s.", autoUpdateFilePath.c_str());
    auto autoUpdateFileDir = autoUpdateCacheDir / md5;
    ret = Platform::UnpackUpgradeTarball(autoUpdateFilePath, autoUpdateFileDir);
    CHECK_ERROR_WITHNOTIFY(ret, resultCallback);

    return 0;
}

int AutoUpdate::checkTarball(const std::filesystem::path& filepath, int64_t size, const std::string& md5)
{
    Log::D(Log::Tag::AU, "Checking tarball from:%s, size:%lld, md5:%s.", filepath.c_str(), size, md5.c_str());
    auto fileExists = std::filesystem::exists(filepath);
    if(fileExists == false) {
        return ErrCode::FileNotExistsError;
    }

    int64_t fileSize = std::filesystem::file_size(filepath);
    CHECK_ASSERT(fileSize == size, ErrCode::BadFileSize);

    auto fileMd5 = MD5::Get(filepath);
    CHECK_ASSERT(fileMd5 == md5, ErrCode::BadFileMd5);

    return 0;
}

} // namespace trinity
