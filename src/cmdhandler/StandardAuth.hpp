#ifndef _FEEDS_STANDARD_AUTH_HPP_
#define _FEEDS_STANDARD_AUTH_HPP_

#include <map>
#include <memory>
#include <vector>

#include <CommandHandler.hpp>

struct DID;
struct DIDDocument;

namespace trinity {

class StandardAuth : public CommandHandler::Listener {
public:
    /*** type define ***/

    /*** static function and variable ***/
    static constexpr const char* LocalDocDirName = "didlocaldoc";

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
    };

    struct AuthSecret {
        std::string did;
        int64_t expiration;
    };

    struct CredentialInfo {
        std::string appDid;
        std::string userDid;
        std::string instanceDid;
        int64_t expiration;

        std::string name;
        std::string email;
    };

    /*** static function and variable ***/
    static std::filesystem::path GetLocalDocDir();
    static int SaveLocalDIDDocument(DID* did, DIDDocument* doc);
    static DIDDocument* LoadLocalDIDDocument(DID* did);

    constexpr static const int64_t JWT_EXPIRATION = (static_cast<int64_t>(5) * 60); // 5 minute
    constexpr static const int64_t ACCESS_EXPIRATION = (static_cast<int64_t>(30) * 24 * 60 * 60); //1 month

    /*** class function and variable ***/
    int onStandardSignIn(const std::string& from,
                         std::shared_ptr<Rpc::Request> request,
                         std::vector<std::shared_ptr<Rpc::Response>>& responseArray);
    int onStandardDidAuth(const std::string& from,
                          std::shared_ptr<Rpc::Request> request,
                          std::vector<std::shared_ptr<Rpc::Response>>& responseArray);

    std::string getServiceDid();
    void cleanExpiredChallenge();
    int makeJwt(time_t expiration,
                const std::string& audience,
                const std::string& subject,
                const std::map<const char*, std::string>& claimMap,
                const std::map<const char*, int>& claimIntMap,
                std::string& jwt);
    int checkAuthToken(const std::string& userName, const std::string& jwtVP,
                       CredentialInfo& credentialInfo);
    int adaptOldLogin(const CredentialInfo& credentialInfo);
    int createAccessToken(const CredentialInfo& credentialInfo,
                          int userIndex,
                          std::string& accessToken);

    std::filesystem::path localDocDir;
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


