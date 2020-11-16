#ifndef _MASSDATA_PROCESSOR_HPP_
#define _MASSDATA_PROCESSOR_HPP_

#include <cassert>
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <CarrierSession.hpp>
#include <MassData.hpp>
#include <SessionParser.hpp>
#include <StdFileSystem.hpp>

extern "C" {
#include <obj.h>
#include <rpc.h>
}

struct ElaCarrier;
struct ElaSession;

namespace trinity {

class MassDataProcessor : public std::enable_shared_from_this<MassDataProcessor>,
                          private MassData {
public:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit MassDataProcessor(const std::filesystem::path& massDataDir);
    virtual ~MassDataProcessor();

    int dispose(const std::vector<uint8_t>& headData,
                const std::filesystem::path& bodyPath);

    int getResultAndReset(std::vector<uint8_t>& headData,
                          std::filesystem::path& bodyPath);

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/
    struct Handler {
        std::function<int(const std::shared_ptr<Req> &,
                          const std::filesystem::path &,
                          std::shared_ptr<Resp> &)> callback;
        Accessible accessible;
    };


    /*** static function and variable ***/

    /*** class function and variable ***/
    int onSetBinary(const std::shared_ptr<Req>& req, const std::filesystem::path& bodyPath, std::shared_ptr<Resp>& resp);
    int onGetBinary(const std::shared_ptr<Req>& req, const std::filesystem::path& bodyPath, std::shared_ptr<Resp>& resp);

    int isOwner(const std::string& accessToken);
    int isMember(const std::string& accessToken);
    int getUserInfo(const std::string& accessToken, std::shared_ptr<UserInfo>& userInfo);

    std::map<const char*, Handler> mothodHandleMap;

    std::vector<uint8_t> resultHeadData;
    std::filesystem::path resultBodyPath;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _MASSDATA_PROCESSOR_HPP_ */


