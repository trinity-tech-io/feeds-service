#ifndef _FEEDS_RPC_FACTORY_HPP_
#define _FEEDS_RPC_FACTORY_HPP_

#include <map>
#include <string>
#include <vector>
#include <RpcDeclare.hpp>

namespace trinity {
namespace Rpc {

class Factory {
public:
    /*** type define ***/
    struct Method {
        static constexpr const char* GetMultiComments = "get_multi_comments";
    };

    /*** static function and variable ***/
    static std::shared_ptr<Request> MakeRequest(const std::string& method);
    static std::shared_ptr<Response> MakeResponse(const std::string& method);

    static int Unmarshal(const std::vector<uint8_t>& data, std::shared_ptr<Request>& request);
    static int Marshal(const std::shared_ptr<Response>& response, std::vector<uint8_t>& data);

    static constexpr const int MaxAvailableSize = 4 * 1024; // 4KB

    /*** class function and variable ***/

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/
    using Dict = std::unordered_map<std::string, msgpack::object>;

    /*** static function and variable ***/
    static constexpr const char* DictKeyMethod = "method";

    /*** class function and variable ***/
    explicit Factory() = delete;
    virtual ~Factory() = delete;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace Rpc
} // namespace trinity

#endif /* _FEEDS_RPC_FACTORY_HPP_ */


