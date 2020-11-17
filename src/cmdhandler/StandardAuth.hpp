#ifndef _FEEDS_STANDARD_AUTH_HPP_
#define _FEEDS_STANDARD_AUTH_HPP_

#include <map>
#include <memory>
#include <vector>

#include <CommandHandler.hpp>

extern "C" {
#include "obj.h"
#include "rpc.h"
}

struct ElaCarrier;
struct ElaSession;
struct ElaStreamCallbacks;

namespace trinity {

class StandardAuth : public CommandHandler::Listener {
public:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit StandardAuth() = default;
    virtual ~StandardAuth() = default;

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/
    struct Method {
        static constexpr const char* SignIn = "standard_sign_in";
        static constexpr const char* DidAuth = "standard_did_auth";
    };

    struct AuthSecret {
        std::string nonce;
        time_t expiration;
    };

    /*** static function and variable ***/
    constexpr static const int DID_EXPIRATION = (3600 * 24 * 60);

    /*** class function and variable ***/
    int onSignIn(std::shared_ptr<Req> req, std::shared_ptr<Resp>& resp);
    int onDidAuth(std::shared_ptr<Req> req, std::shared_ptr<Resp>& resp);

    int checkAuthToken(const char* jwt);

    std::map<std::string, AuthSecret> authSecretMap;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _FEEDS_STANDARD_AUTH_HPP_ */


