#ifndef _MASSDATA_MANAGER_HPP_
#define _MASSDATA_MANAGER_HPP_

#include <memory>
#include <map>
#include <string>
#include <vector>
#include <CarrierSessionHelper.hpp>
#include <SessionParser.hpp>
#include <StdFileSystem.hpp>

struct Carrier;
struct ElaSession;

namespace trinity {

class MassDataProcessor;

class MassDataManager : public std::enable_shared_from_this<MassDataManager> {
public:
    /*** type define ***/

    /*** static function and variable ***/
    static std::shared_ptr<MassDataManager> GetInstance();

    /*** class function and variable ***/
    int config(const std::filesystem::path& dataDir,
               std::weak_ptr<Carrier> carrier);
    void cleanup();

    void removeDataPipe(const std::string& key);
    void clearAllDataPipe();

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/
    struct DataPipe {
        std::shared_ptr<CarrierSessionHelper> session;
        std::shared_ptr<SessionParser> parser;
        std::shared_ptr<MassDataProcessor> processor;
    };

    /*** static function and variable ***/
    static std::shared_ptr<MassDataManager> MassDataMgrInstance;

    /*** class function and variable ***/
    explicit MassDataManager() = default;
    virtual ~MassDataManager() = default;

    void onSessionRequest(std::weak_ptr<Carrier> carrier,
                          const std::string& from, const std::string& sdp);
                        
    void appendDataPipe(const std::string& key, std::shared_ptr<DataPipe> value);
    std::shared_ptr<DataPipe> find(const std::string& key);

    std::shared_ptr<CarrierSessionHelper::ConnectListener> makeConnectListener(const std::string& peerId,
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

} // namespace trinity

#endif /* _MASSDATA_MANAGER_HPP_ */


