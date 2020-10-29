#ifndef _MASSDATA_PROCESSOR_HPP_
#define _MASSDATA_PROCESSOR_HPP_

#include <cassert>
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <CarrierSession.hpp>
#include <SessionParser.hpp>
#include <StdFileSystem.hpp>
extern "C" {
#include <obj.h>
#include <rpc.h>
}

struct ElaCarrier;
struct ElaSession;

namespace trinity {

class MassDataProcessor : public std::enable_shared_from_this<MassDataProcessor> {
public:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit MassDataProcessor();
    virtual ~MassDataProcessor();

    void config(const std::filesystem::path& massDataDir);

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
    struct Method {
        static constexpr const char* SetBinary = "set_binary";
        static constexpr const char* GetBinary = "get_binary";
    };
    using Handler = std::function<int(std::shared_ptr<Req>,
                                  const std::filesystem::path&,
                                  std::shared_ptr<Resp>&)>;

    /*** static function and variable ***/

    /*** class function and variable ***/
    int isOwner(const std::string& accessToken);
    int isMember(const std::string& accessToken);
    int getUserInfo(const std::string& accessToken, std::shared_ptr<UserInfo>& userInfo);

    int onSetBinary(std::shared_ptr<Req> req, const std::filesystem::path& bodyPath, std::shared_ptr<Resp>& resp);
    int onGetBinary(std::shared_ptr<Req> req, const std::filesystem::path& bodyPath, std::shared_ptr<Resp>& resp);

    std::filesystem::path massDataDir;
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


