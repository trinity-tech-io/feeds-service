#include "StandardAuth.hpp"

#include <map>

#include <ErrCode.hpp>
#include <Log.hpp>
#include <SafePtr.hpp>

extern "C" {
#define new fix_cpp_keyword_new
#include <crystal.h>
#include <ela_jwt.h>
#include <ela_did.h>
#include "../did.h"
#undef new
//} // ela_jwt error

namespace trinity {

#define CHECK_DIDSDK(expr, errCode, errDesp) \
    if(!(expr)) { \
        Log::E(Log::TAG, errDesp " diderr=0x%x, desc=%s", \
               DIDError_GetCode(), DIDError_GetMessage()); \
        CHECK_ERROR(errCode); \
    }

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */
int StandardAuth::onDispose(std::shared_ptr<Req> req, std::shared_ptr<Resp> &resp)
{
    using namespace std::placeholders;
    using Handler = std::function<int(std::shared_ptr<Req>, std::shared_ptr<Resp> &)>;

    const std::map<const char*, Handler> handleMap = {
        {Method::SignIn, std::bind(&StandardAuth::onSignIn, this, _1, _2)},
        {Method::DidAuth, std::bind(&StandardAuth::onDidAuth, this, _1, _2)},
    };


    for (const auto& it : handleMap) {
        if (std::strcmp(it.first, req->method) != 0) {
            continue;
        }

        int ret = it.second(req, resp);
        return ret;
    }

    return ErrCode::UnimplementedError;
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
int StandardAuth::onSignIn(std::shared_ptr<Req> req,
                           std::shared_ptr<Resp> &resp)
{
    auto signInReq = std::reinterpret_pointer_cast<StandardSignInReq>(req);
    Log::D(Log::TAG, "Request params:");
    Log::D(Log::TAG, "    document: %s", signInReq->params.doc);

    auto docCreater = [](const char* json) -> DIDDocument* {
        return DIDDocument_FromJson(json);
    };
    auto docDeleter = [](DIDDocument* ptr) -> void {
        DIDDocument_Destroy(ptr);
    };
    auto didDoc = std::shared_ptr<DIDDocument>(docCreater(signInReq->params.doc), docDeleter);
    CHECK_DIDSDK(didDoc != nullptr, ErrCode::AuthBadDidDoc, "Failed to get did document from json.");

    bool ret = DIDDocument_IsValid(didDoc.get());
    CHECK_DIDSDK(ret, ErrCode::AuthDidDocInvlid, "Did document is invalid.");

    auto did = DIDDocument_GetSubject(didDoc.get());
    CHECK_DIDSDK(did, ErrCode::AuthBadDid, "Failed to get did from document.");

    auto didSpec = DID_GetMethodSpecificId(did);
    CHECK_DIDSDK(didSpec, ErrCode::AuthBadDid, "Failed to get did specific from did");

    auto didMtd = DID_GetMethod(did);
    CHECK_DIDSDK(didMtd, ErrCode::AuthBadDidMethod, "Failed to get did method from did");

    auto didStr = std::string("did:") + didMtd + ":" + didSpec;
    Log::D(Log::TAG, "Sign in Did: %s", didStr.c_str());

    uint8_t nonce[NONCE_BYTES];
    char nonceStr[NONCE_BYTES << 1];
    crypto_random_nonce(nonce);
    crypto_nonce_to_str(nonce, nonceStr, sizeof(nonceStr));

    auto expiration = time(NULL) + DID_EXPIRATION;
    authSecretMap[didStr] = std::move(AuthSecret{nonceStr, expiration});

    auto jwtCreater = [](DIDDocument* didDoc) -> JWTBuilder* {
        return DIDDocument_GetJwtBuilder(didDoc);
    };
    auto jwtDeleter = [](JWTBuilder* ptr) -> void {
        JWTBuilder_Destroy(ptr);
    };
    auto jwtBuilder = std::shared_ptr<JWTBuilder>(jwtCreater(feeds_doc), jwtDeleter);
    CHECK_DIDSDK(jwtBuilder != nullptr, ErrCode::AuthBadJwtBuilder, "Failed to get jwt builder from self did");

    ret = JWTBuilder_SetHeader(jwtBuilder.get(), "type", "JWT");
    CHECK_DIDSDK(ret, ErrCode::AuthBadJwtHeader, "Failed to set jwt header.");

    ret = JWTBuilder_SetHeader(jwtBuilder.get(), "version", "1.0");
    CHECK_DIDSDK(ret, ErrCode::AuthBadJwtHeader, "Failed to set jwt header.");

    ret = JWTBuilder_SetSubject(jwtBuilder.get(), "DIDAuthChallenge");
    CHECK_DIDSDK(ret, ErrCode::AuthBadJwtSubject, "Failed to set jwt subject.");

    ret = JWTBuilder_SetAudience(jwtBuilder.get(), didStr.c_str());
    CHECK_DIDSDK(ret, ErrCode::AuthBadJwtAudience, "Failed to set jwt audience.");

    ret = JWTBuilder_SetClaim(jwtBuilder.get(), "nonce", nonceStr);
    CHECK_DIDSDK(ret, ErrCode::AuthBadJwtClaim, "Failed to set jwt claim.");

    ret = JWTBuilder_SetExpiration(jwtBuilder.get(), expiration);
    CHECK_DIDSDK(ret, ErrCode::AuthBadJwtExpiration, "Failed to set jwt expiration.");

    int jwtSigned = JWTBuilder_Sign(jwtBuilder.get(), feeeds_auth_key_url, feeds_storepass);
    CHECK_DIDSDK(jwtSigned == 0, ErrCode::AuthJwtSignFailed, "Failed to sign jwt.");

    auto tokenCreater = [](JWTBuilder* jwtBuilder) -> const char* {
        return JWTBuilder_Compact(jwtBuilder);
    };
    auto tokenDeleter = [](const char* ptr) -> void {
        if(ptr != nullptr) {
            free(const_cast<char*>(ptr));
        }
    };
    auto token = std::shared_ptr<const char>(tokenCreater(jwtBuilder.get()), tokenDeleter);
    CHECK_DIDSDK(token != nullptr, ErrCode::AuthJwtCompactFailed, "Failed to compact jwt.");

    auto signInResp = std::make_shared<StandardSignInResp>();
    signInResp->tsx_id = signInReq->tsx_id;
    auto tokenSize = std::strlen(token.get()) + 1;
    signInResp->result.challenge = (char*)rc_zalloc(tokenSize, nullptr);
    strncpy(signInResp->result.challenge, token.get(), tokenSize);
    Log::D(Log::TAG, "Response result:");
    Log::D(Log::TAG, "    challenge: %s", signInResp->result.challenge);

    resp = std::reinterpret_pointer_cast<Resp>(signInResp);

    return 0;
}

int StandardAuth::onDidAuth(std::shared_ptr<Req> req,
                            std::shared_ptr<Resp> &resp)
{
    auto didAuthReq = std::reinterpret_pointer_cast<StandardDidAuthReq>(req);
    Log::D(Log::TAG, "Request params:");
    Log::D(Log::TAG, "    challenge: %s", didAuthReq->params.challenge);

    auto credentialSubject = checkAuthToken(didAuthReq->params.challenge);

    return -1;
}

int StandardAuth::checkAuthToken(const char* jwt)
{
    CHECK_ASSERT(jwt, ErrCode::InvalidArgument);

    auto jws = DefaultJWSParser_Parse(jwt);

}

} // namespace trinity