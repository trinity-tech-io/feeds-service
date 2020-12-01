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
    struct Method {
        static constexpr const char* GetMultiComments = "get_multi_comments";
    };

    /*** static function and variable ***/

    /*** class function and variable ***/
    int onGetMultiComments(std::shared_ptr<Req> req, std::vector<std::shared_ptr<Resp>>& resp);
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _FEEDS_CHANNEL_METHOD_HPP_ */


