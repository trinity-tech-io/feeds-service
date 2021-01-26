#ifndef _FEEDS_SERVICE_METHOD_HPP_
#define _FEEDS_SERVICE_METHOD_HPP_

#include <map>
#include <memory>
#include <vector>

#include <CommandHandler.hpp>

namespace trinity {

class ServiceMethod : public CommandHandler::Listener {
public:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit ServiceMethod(const std::filesystem::path& cacheDir);
    virtual ~ServiceMethod();

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    int onBackupServiceData(const std::string& from,
                            std::shared_ptr<Rpc::Request> request,
                            std::vector<std::shared_ptr<Rpc::Response>>& responseArray);

    std::filesystem::path cacheDir;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _FEEDS_SERVICE_METHOD_HPP_ */


