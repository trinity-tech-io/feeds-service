#ifndef _FEEDS_STANDARD_AUTH_HPP_
#define _FEEDS_STANDARD_AUTH_HPP_

#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <vector>

#include <CommandHandler.hpp>

using nlohmann::json;

namespace trinity {

class StandardAuth : public CommandHandler::Listener {
public:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit StandardAuth();
    virtual ~StandardAuth();

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
        std::string did;
        time_t expiration;
    };

    /*** static function and variable ***/
    constexpr static const int ACCESS_EXPIRATION = (3600 * 24 * 60);

    /*** class function and variable ***/
    int onSignIn(std::shared_ptr<Req> req, std::shared_ptr<Resp>& resp);
    int onDidAuth(std::shared_ptr<Req> req, std::shared_ptr<Resp>& resp);

    int checkAuthToken(const char* jwt, json& credentialSubject);
    int createAccessToken(json& credentialSubject, std::shared_ptr<const char>& accessToken);

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


