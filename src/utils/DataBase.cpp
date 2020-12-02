#include "DataBase.hpp"

#include <ErrCode.hpp>
#include <Log.hpp>

extern "C" {
#include <db.h>
}


namespace trinity {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */
std::shared_ptr<DataBase> DataBase::DataBaseInstance;

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */
std::shared_ptr<DataBase> DataBase::GetInstance()
{
    // ignore check thread-safty, no needed

    if(DataBaseInstance != nullptr) {
        return DataBaseInstance;
    }

    struct Impl: DataBase {
    };
    DataBaseInstance = std::make_shared<Impl>();

    return DataBaseInstance;
}


/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */
int DataBase::config(const std::filesystem::path& databaseFilePath)
{
    handler = std::make_shared<SQLite::Database>(databaseFilePath, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
    CHECK_ASSERT(handler != nullptr, ErrCode::DBOpenFailed);

    int ret = db_init(handler->getHandle());
    if(ret < 0) {
        CHECK_ERROR(ErrCode::DBInitFailed);
    }

    return 0;
}

void DataBase::cleanup()
{
    db_deinit();
    DataBaseInstance.reset();
}

std::shared_ptr<SQLite::Database> DataBase::getHandler()
{
    return handler;
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */

} // namespace trinity
