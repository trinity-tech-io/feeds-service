#ifndef _FEEDS_LEGACY_METHOD_HPP_
#define _FEEDS_LEGACY_METHOD_HPP_

#include <map>
#include <memory>
#include <vector>

#include <CommandHandler.hpp>

namespace trinity {

class LegacyMethod : public CommandHandler::Listener {
public:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit LegacyMethod() = default;
    virtual ~LegacyMethod() = default;

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    virtual int onDispose(const std::string& from,
                          std::shared_ptr<Req> req,
                          std::shared_ptr<Resp>& resp) override final;

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _FEEDS_LEGACY_METHOD_HPP_ */


