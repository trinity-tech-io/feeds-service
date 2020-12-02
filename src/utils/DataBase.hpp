#ifndef _FEEDS_DATABASE_HPP_
#define _FEEDS_DATABASE_HPP_

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <StdFileSystem.hpp>
#include <SQLiteCpp/SQLiteCpp.h>

namespace trinity {

class DataBase {
public:
    /*** type define ***/
    enum QueryField {
        Id = 1,
        UpdatedAt = 2,
        CreatedAt = 3,
    };
    enum QueryIdType {
        Channel = 1,
        Post = 2,
        Comment = 3,
    };

    /*** static function and variable ***/
    static std::shared_ptr<DataBase> GetInstance();
    static const char* QueryBy(QueryField field, QueryIdType idType);

    /*** class function and variable ***/
    int config(const std::filesystem::path& databaseFilePath);
    void cleanup();

    std::shared_ptr<SQLite::Database> getHandler();

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/
    static std::shared_ptr<DataBase> DataBaseInstance;

    /*** class function and variable ***/
    explicit DataBase() = default;
    virtual ~DataBase() = default;
    std::shared_ptr<SQLite::Database> handler;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _FEEDS_DATABASE_HPP_ */


