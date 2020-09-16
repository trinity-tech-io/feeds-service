#ifndef _MASSDATA_MANAGER_HPP_
#define _MASSDATA_MANAGER_HPP_

#include <filesystem>
#include <memory>
#include <map>
#include <string>
#include <vector>

struct ElaCarrier;
struct ElaSession;

namespace elastos {

class CarrierSession;
class SessionParser;
class MassDataProcessor;

class MassDataManager : public std::enable_shared_from_this<MassDataManager> {
public:
    /*** type define ***/

    /*** static function and variable ***/
    static std::shared_ptr<MassDataManager> GetInstance();

    /*** class function and variable ***/
    int config(const std::filesystem::path& dataDir,
               std::weak_ptr<ElaCarrier> carrier);
    void cleanup();

    int sendData(const std::string& to,
                 const std::vector<uint8_t>& headData,
                const std::filesystem::path& bodyPath);

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/
    struct DataPipe {
        std::shared_ptr<CarrierSession> session;
        std::shared_ptr<SessionParser> parser;
        std::shared_ptr<MassDataProcessor> processor;
    };

    /*** static function and variable ***/
    static constexpr const char* MassDataDirName = "massdata";
    static constexpr const char* MassDataCacheDirName = "cache";
    static std::shared_ptr<MassDataManager> MassDataMgrInstance;

    /*** class function and variable ***/
    explicit MassDataManager() = default;
    virtual ~MassDataManager() = default;

    void onSessionRequest(std::weak_ptr<ElaCarrier> carrier,
                          const std::string& from, const std::string& sdp);

    std::filesystem::path massDataDir;
    std::map<std::string, std::shared_ptr<DataPipe>> dataPipeMap;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace elastos

#endif /* _MASSDATA_MANAGER_HPP_ */


