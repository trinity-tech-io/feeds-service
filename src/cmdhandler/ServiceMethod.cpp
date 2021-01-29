#include "ServiceMethod.hpp"

#include <fstream>
#include <json.hpp>
#include <CloudDrive.hpp>
#include <ErrCode.hpp>
#include <Log.hpp>
#include <MD5.hpp>
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
            Rpc::Factory::Method::BackupServiceData,
            {std::bind(&ServiceMethod::onBackupServiceData, this, _1, _2, _3), Accessible::Owner}
        },
        {
            Rpc::Factory::Method::RestoreServiceData,
            {std::bind(&ServiceMethod::onRestoreServiceData, this, _1, _2, _3), Accessible::Owner}
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

    const std::set<std::string> migrateDirSet = {
        "carrier",
        "db",
        "didlocaldoc",
        "didstore",
        "massdata",
    };

    const auto badType = static_cast<CloudDrive::Type>(-1);
    auto type = badType;
    if(params.drive_name == "OneDrive") {
        type = CloudDrive::Type::OneDrive;
    }
    CHECK_ASSERT(type != badType, ErrCode::InvalidParams);
    
    auto drive = CloudDrive::Create(type, params.drive_url, params.drive_dir, params.drive_access_token);
    CHECK_ASSERT(drive != nullptr, ErrCode::InvalidParams);

    int ret = drive->remove("");
    CHECK_ERROR(ret);

    std::map<std::string, std::string> fileHashMap;
    auto dataPath = std::filesystem::path(dataDir);
    for(const auto& migrateDir: migrateDirSet) {
        auto migratePath = dataPath / migrateDir;
        for(const auto& itemPath: std::filesystem::recursive_directory_iterator(migratePath)) {
            if(std::filesystem::is_directory(itemPath) == true) {
                continue;
            }
            if(std::filesystem::file_size(itemPath) == 0) {
                Log::I(Log::Tag::Cmd, "Backup service data, ignore empty file: %s.", itemPath.path().c_str());
                continue;
            }

            auto filePath = itemPath.path();
            Log::I(Log::Tag::Cmd, "Backup service data, uploading %s.", filePath.c_str());
            auto fileStream = std::make_shared<std::fstream>();
            fileStream->open(filePath, std::ios::binary | std::ios::in | std::ios::out);
            auto relativePath = filePath.lexically_relative(dataPath);
            ret = drive->write(relativePath.string(), fileStream);
            fileStream->close();
            CHECK_ERROR(ret);

            fileHashMap[relativePath.string()] = MD5::Get(filePath); 

        }
    }
    
    auto fileHashStream = std::make_shared<std::stringstream>();
    auto fileHashJson = nlohmann::json(fileHashMap);
    *fileHashStream << fileHashJson.dump(2);
    ret = drive->write(BACKUP_LIST_FILENAME, fileHashStream);
    CHECK_ERROR(ret);

    auto responsePtr = Rpc::Factory::MakeResponse(request->method);
    auto response = std::dynamic_pointer_cast<Rpc::BackupServiceDataResponse>(responsePtr);
    CHECK_ASSERT(response != nullptr, ErrCode::RpcUnimplementedError);
    response->version = request->version;
    response->id = request->id;
    responseArray.push_back(response);

    return 0;
}

int ServiceMethod::onRestoreServiceData(const std::string& from,
                                       std::shared_ptr<Rpc::Request> request,
                                       std::vector<std::shared_ptr<Rpc::Response>>& responseArray)
{
    auto requestPtr = std::dynamic_pointer_cast<Rpc::RestoreServiceDataRequest>(request);
    CHECK_ASSERT(requestPtr != nullptr, ErrCode::InvalidArgument);
    const auto& params = requestPtr->params;
    responseArray.clear();

    const auto badType = static_cast<CloudDrive::Type>(-1);
    auto type = badType;
    if(params.drive_name == "OneDrive") {
        type = CloudDrive::Type::OneDrive;
    }
    CHECK_ASSERT(type != badType, ErrCode::InvalidParams);
    
    auto drive = CloudDrive::Create(type, params.drive_url, params.drive_dir, params.drive_access_token);
    CHECK_ASSERT(drive != nullptr, ErrCode::InvalidParams);

    auto fileHashStream = std::make_shared<std::stringstream>();
    int ret = drive->read(BACKUP_LIST_FILENAME, fileHashStream);
    CHECK_ERROR(ret);

    auto fileHashJson = nlohmann::json::parse(fileHashStream->str());
    auto fileHashMap = fileHashJson.get<std::map<std::string, std::string>>();;

    auto restoreCacheDir = cacheDir / "migrate-data";
    std::error_code ec;
    std::filesystem::remove_all(restoreCacheDir, ec); // noexcept
    auto dirExists = std::filesystem::create_directories(restoreCacheDir);
    CHECK_ASSERT(dirExists, ErrCode::FileNotExistsError);

    for(const auto& it: fileHashMap) {
        auto relativePath = it.first;
        auto filePath = restoreCacheDir / relativePath;
        auto fileMd5 = it.second;

        dirExists = std::filesystem::exists(filePath.parent_path());
        if(dirExists == false) {
            dirExists = std::filesystem::create_directories(filePath.parent_path());
        }
        CHECK_ASSERT(dirExists, ErrCode::FileNotExistsError);

        auto fileStream = std::make_shared<std::fstream>();
        fileStream->open(filePath, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        ret = drive->read(relativePath, fileStream);
        fileStream->flush();
        fileStream->close();
        CHECK_ERROR(ret);

        CHECK_ASSERT(fileMd5 == MD5::Get(filePath), ErrCode::BadFileMd5);
    }

    auto responsePtr = Rpc::Factory::MakeResponse(request->method);
    auto response = std::dynamic_pointer_cast<Rpc::RestoreServiceDataResponse>(responsePtr);
    CHECK_ASSERT(response != nullptr, ErrCode::RpcUnimplementedError);
    response->version = request->version;
    response->id = request->id;
    responseArray.push_back(response);

    return 0;
}

} // namespace trinity