#ifndef _FEEDS_MASS_DATA_HPP_
#define _FEEDS_MASS_DATA_HPP_

#include <map>
#include <memory>
#include <vector>

#include <CommandHandler.hpp>

namespace trinity {

class MassData : public CommandHandler::Listener {
public:
    /*** type define ***/
    struct Method {
        static constexpr const char* SetBinary = "set_binary";
        static constexpr const char* GetBinary = "get_binary";
    };

    /*** static function and variable ***/
    static constexpr const char* MassDataDirName = "massdata";
    static constexpr const char* MassDataCacheDirName = "cache";
    static constexpr const char* MassDataCacheName = "massdata-cache-";

    /*** class function and variable ***/
    explicit MassData(const std::filesystem::path& massDataDir);
    virtual ~MassData();

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

    int saveBinData(const std::filesystem::path &massDataDir,
                   const std::shared_ptr<Req>& req,
                   std::shared_ptr<Resp> &resp,
                   const std::filesystem::path &contentFilePath);
    int loadBinData(const std::filesystem::path &massDataDir,
                    const std::shared_ptr<Req>& req,
                    std::shared_ptr<Resp> &resp,
                    std::filesystem::path &contentFilePath);

    std::filesystem::path massDataDir;

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    int onSetBinary(std::shared_ptr<Req> req, std::shared_ptr<Resp>& resp);
    int onGetBinary(std::shared_ptr<Req> req, std::shared_ptr<Resp>& resp);
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _FEEDS_MASS_DATA_HPP_ */


