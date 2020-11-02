#ifndef _MASSDATA_MANAGER_HPP_
#define _MASSDATA_MANAGER_HPP_

#include <filesystem>
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <CarrierSession.hpp>
#include <SessionParser.hpp>

struct ElaCarrier;
struct ElaSession;

namespace elastos {

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
                        
    void append(const std::string& key, std::shared_ptr<DataPipe> value);
    void remove(const std::string& key);
    std::shared_ptr<DataPipe> find(const std::string& key);

    std::shared_ptr<CarrierSession::ConnectListener> makeConnectListener(const std::string& peerId,
                                                                         std::shared_ptr<SessionParser::OnUnpackedListener> unpackedListener);
    std::shared_ptr<SessionParser::OnUnpackedListener> makeUnpackedListener(const std::string& peerId);

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


