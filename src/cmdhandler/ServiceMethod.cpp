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
ServiceMethod::ServiceMethod(const std::filesystem::path& dataDir)
    : dataDir(dataDir)
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
    *fileHashStream << nlohmann::json(fileHashMap).dump(2);
    ret = drive->write("backup-list.json", fileHashStream);
    CHECK_ERROR(ret);

    return 0;
}

} // namespace trinity