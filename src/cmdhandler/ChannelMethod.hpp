#ifndef _FEEDS_CHANNEL_METHOD_HPP_
#define _FEEDS_CHANNEL_METHOD_HPP_

#include <map>
#include <memory>
#include <vector>

#include <CommandHandler.hpp>

namespace trinity {

class ChannelMethod : public CommandHandler::Listener {
public:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit ChannelMethod();
    virtual ~ChannelMethod();

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    int onGetMultiComments(const std::string& from,
                           std::shared_ptr<Rpc::Request> request,
                           std::vector<std::shared_ptr<Rpc::Response>>& responseArray);
    int onGetMultiLikesAndCommentsCount(const std::string& from,
                                        std::shared_ptr<Rpc::Request> request,
                                        std::vector<std::shared_ptr<Rpc::Response>> &responseArray);
    int onGetMultiSubscribersCount(const std::string& from,
                                   std::shared_ptr<Rpc::Request> request,
                                   std::vector<std::shared_ptr<Rpc::Response>> &responseArray);
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _FEEDS_CHANNEL_METHOD_HPP_ */


