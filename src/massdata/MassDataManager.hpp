#ifndef _MASSDATA_MANAGER_HPP_
#define _MASSDATA_MANAGER_HPP_

#include <future> 
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <CarrierSession.hpp>

struct ElaCarrier;
struct ElaSession;

namespace elastos {

class MassDataManager : public std::enable_shared_from_this<MassDataManager> {
public:
    /*** type define ***/
    static std::shared_ptr<MassDataManager> GetInstance();

    /*** static function and variable ***/
    int config(std::weak_ptr<ElaCarrier> carrier);
    void cleanup();

    /*** class function and variable ***/

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/
    static std::shared_ptr<MassDataManager> MassDataMgrInstance;

    /*** class function and variable ***/
    explicit MassDataManager() = default;
    virtual ~MassDataManager() = default;

    void onSessionRequest(std::weak_ptr<ElaCarrier> carrier,
                          const std::string& from, const std::string& sdp);

    std::map<CarrierSession::ConnectListener*, std::shared_ptr<CarrierSession>> carrierSessionMap;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace elastos

#endif /* _MASSDATA_MANAGER_HPP_ */


