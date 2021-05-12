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

const char* DataBase::ConditionBy(ConditionField field, ConditionIdType idType)
{
    switch (field) {
    case Id:
        switch (idType) {
        case Channel:
            return "channel_id";
        case Post:
            return "post_id";
        case Comment:
            return "comment_id";
        default:
            return nullptr;
        }
    case UpdatedAt:
        return "updated_at";
    case CreatedAt:
        return "created_at";
    default:
        return nullptr;
    }
}

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */
int DataBase::config(const std::filesystem::path& databaseFilePath)
{
    Log::D(Log::Tag::Db, "Config database.");

    handler = std::make_shared<SQLite::Database>(databaseFilePath.string().c_str(), SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
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

    Log::D(Log::Tag::Db, "Cleanup database.");
}

std::shared_ptr<SQLite::Database> DataBase::getHandler()
{
    return handler;
}

int DataBase::executeStep(const std::string& sql, Step& step)
{
    try {
        Log::D(Log::Tag::Db, "DataBase sql: %s", sql.c_str());
        SQLite::Statement stmt(*handler, sql);

        while (stmt.executeStep()) {
            int ret = step(stmt);
            CHECK_ERROR(ret);
        }
    } catch (SQLite::Exception& e) {
        Log::E(Log::Tag::Db, "DataBase exec failed. exception: %s", e.what());
        CHECK_ERROR(ErrCode::DBException);
    }

    return 0;
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */

} // namespace trinity
