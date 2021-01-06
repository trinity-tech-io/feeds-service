#include "AutoUpdate.hpp"

#include <fstream>
#include <signal.h>
#include <ver.h>
#include <unistd.h>
#include <ErrCode.hpp>
#include <HttpClient.hpp>
#include <MD5.hpp>
#include <Platform.hpp>
#include <ThreadPool.hpp>

extern "C" {
#define new fix_cpp_keyword_new
#include <did.h>
#undef new
}

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
int AutoUpdate::needUpdate(const std::string& newVerName)
{
    int64_t newVerCode = 0;
    size_t start;
	size_t end = 0;
	while ((start = newVerName.find_first_not_of('.', end)) != std::string::npos) {
		end = newVerName.find('.', start);
        auto subVer = newVerName.substr(start, end - start);
        newVerCode = (newVerCode * 100 + std::stoi(subVer));
	}

    int64_t currVerCode = FEEDSD_VERCODE;
    Log::I(Log::Tag::AU, "Compare old/new version between %lld and %lld", currVerCode, newVerCode);

    if(currVerCode >= newVerCode) {
        return ErrCode::AutoUpdateAlreadyNewest;
    }

    return 0;
}

int AutoUpdate::asyncDownloadTarball(const std::string& verName,
                                     const std::string& url,
                                     const std::filesystem::path& runtimeDir,
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
        downloadTarball(verName, url, runtimeDir, cacheDir,
                        name, size, md5,
                        resultCallback);
    });

    return 0;
}

int AutoUpdate::startTarball(const std::filesystem::path& runtimeDir,
                             const std::filesystem::path& cacheDir,
                             const std::string& verName,
                             const std::string& launchCmd)
{
    auto updateHelper = runtimeDir / verName / "bin" / "feedsd.updatehelper";
    auto fileExists = std::filesystem::exists(updateHelper);
    if(fileExists == false) {
        updateHelper = runtimeDir / "current" / "bin" / "feedsd.updatehelper";
        fileExists = std::filesystem::exists(updateHelper);
    }
    Log::I(Log::Tag::AU, "Update helper is %s", updateHelper.c_str());
    CHECK_ASSERT(fileExists, ErrCode::FileNotExistsError);

    auto autoUpdateCacheDir = cacheDir / "autoupdate";
    auto dirExists = std::filesystem::exists(autoUpdateCacheDir);
    if(dirExists == false) {
        dirExists = std::filesystem::create_directories(autoUpdateCacheDir);
    }
    CHECK_ASSERT(dirExists, ErrCode::FileNotExistsError);

    auto logFileName = std::string("autoupdate.") + verName + ".log";
    auto logFile = autoUpdateCacheDir / logFileName;
    Log::I(Log::Tag::AU, "Update log file is %s", logFile.c_str());

    std::stringstream cmdline;
    cmdline << "'" << updateHelper.string() << "'";
    cmdline << " '" << runtimeDir.string() << "'";
    cmdline << " " << verName;
    cmdline << " " << std::to_string(getpid());
    cmdline << " '" << launchCmd << "'";
    cmdline << " '" << logFile.string() << "'";
    cmdline << "& ";

    did_stop_httpserver();
    int ret = std::system(cmdline.str().c_str());
    CHECK_ASSERT(ret == 0, ErrCode::ExecSystemCommendFailed);

    return 0;
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
int AutoUpdate::downloadTarball(const std::string& verName,
                                const std::string& url,
                                const std::filesystem::path& runtimeDir,
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

    ret = ErrCode::AutoUpdateBadTarball;
    for(const auto& it: std::filesystem::recursive_directory_iterator(autoUpdateFileDir)) {
        if(it.path().filename() != verName) {
            continue;
        }

        ret = 0;
        std::error_code ec;
        std::filesystem::remove_all(runtimeDir / verName, ec); // noexcept
        using std::filesystem::copy_options;
        const auto options = std::filesystem::copy_options::update_existing
                           | std::filesystem::copy_options::recursive;
        std::filesystem::copy(it.path(), runtimeDir / verName, options, ec); // noexcept
        if (ec.value() != 0) {
            Log::E(Log::Tag::AU, "Failed to move %s ==> %s. err: %s(%d)",
                                 it.path().c_str(), (runtimeDir / verName).c_str(),
                                 ec.message().c_str(), ec.value());
            ret = ErrCode::AutoUpdateMoveTarballFailed;
        }
        break;
    }
    CHECK_ERROR_WITHNOTIFY(ret, resultCallback);

    resultCallback(0);
    return 0;
}

int AutoUpdate::checkTarball(const std::filesystem::path& filepath, int64_t size, const std::string& md5)
{
    Log::D(Log::Tag::AU, "Checking tarball from:%s, size:%lld, md5:%s.", filepath.c_str(), size, md5.c_str());
    auto fileExists = std::filesystem::exists(filepath);
    if(fileExists == false) {
        return ErrCode::FileNotExistsError;
    }

    std::error_code ec;
    int64_t fileSize = std::filesystem::file_size(filepath, ec);
    if (ec.value() != 0) {
        Log::E(Log::Tag::AU, "Failed get file size %s. err: %s(%d)",
                                filepath.c_str(), ec.message().c_str(), ec.value());
    }
    CHECK_ASSERT(fileSize == size, ErrCode::BadFileSize);

    auto fileMd5 = MD5::Get(filepath);
    CHECK_ASSERT(fileMd5 == md5, ErrCode::BadFileMd5);

    return 0;
}

} // namespace trinity
