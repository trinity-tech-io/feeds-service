#include <ErrCode.hpp>

#include <map>
#include <system_error>

namespace trinity {

/***********************************************/
/***** static variables initialize *************/
/***********************************************/
std::function<void(int, const std::string&, const std::string&)> ErrCode::sErrorListener;


/***********************************************/
/***** static function implement ***************/
/***********************************************/
void ErrCode::SetErrorListener(std::function<void(int, const std::string&, const std::string&)> listener)
{
    sErrorListener = listener;
}

void ErrCode::SetError(int errCode, const std::string& ext)
{
    if(errCode >= 0) {
        return;
    }
    if(!sErrorListener) {
        return;
    }

    auto errStr = ToString(errCode);
    sErrorListener(errCode, errStr, ext);
}

std::string ErrCode::ToString(int errCode)
{
    if(errCode > ReservedError) {
        return err_strerror(errCode);
    }

    static const std::map<int, const char*> errDespMap = {
        { UnknownError                     , "UnknownError" },
        { UnimplementedError               , "UnimplementedError" },
        { NotFoundError                    , "NotFoundError" },
        { InvalidArgument                  , "InvalidArgument" },
        { PointerReleasedError             , "PointerReleasedError" },
        { DevUUIDError                     , "DevUUIDError" },
        { FileNotExistsError               , "FileNotExistsError" },
        { CreateDirectoryError             , "CreateDirectoryError" },
        { SizeOverflowError                , "SizeOverflowError" },
        { StdSystemError                   , "StdSystemError" },
        { OutOfMemoryError                 , "OutOfMemoryError" },
        { CompletelyFinishedNotify         , "CompletelyFinishedNotify" },

        { DidNotReady                      , "DidNotReady" },
        { InvalidAccessToken               , "InvalidAccessToken" },
        { NotAuthorizedError               , "NotAuthorizedError" },

        { CarrierSessionInitFailed         , "CarrierSessionInitFailed" },
        { CarrierSessionConnectFailed      , "CarrierSessionConnectFailed" },
        { CarrierSessionCreateFailed       , "CarrierSessionCreateFailed" },
        { CarrierSessionAddStreamFailed    , "CarrierSessionAddStreamFailed" },
        { CarrierSessionTimeoutError       , "CarrierSessionTimeoutError" },
        { CarrierSessionReplyFailed        , "CarrierSessionReplyFailed" },
        { CarrierSessionStartFailed        , "CarrierSessionStartFailed" },
        { CarrierSessionBadStatus          , "CarrierSessionBadStatus" },
        { CarrierSessionDataNotEnough      , "CarrierSessionDataNotEnough" },
        { CarrierSessionUnsuppertedVersion , "CarrierSessionUnsuppertedVersion" },
        { CarrierSessionReleasedError      , "CarrierSessionReleasedError" },
        { CarrierSessionSendFailed         , "CarrierSessionSendFailed" },
        { CarrierSessionErrorExists        , "CarrierSessionErrorExists" },

        { CmdUnknownReqFailed              , "CmdUnknownReqFailed" },
        { CmdUnmarshalReqFailed            , "CmdUnmarshalReqFailed" },
        { CmdMarshalRespFailed             , "CmdMarshalRespFailed" },
        { CmdUnsupportedVersion            , "CmdUnsupportedVersion" },
        { CmdUnsupportedAlgo               , "CmdUnsupportedAlgo" },
        { CmdUnknownRespFailed             , "CmdUnknownRespFailed" },

        { AuthBadDidDoc                        , "AuthBadDidDoc" },
        { AuthDidDocInvlid                     , "AuthDidDocInvlid" },
        { AuthBadDid                           , "AuthBadDid" },
        { AuthBadDidString                     , "AuthBadDidString" },
        { AuthSaveDocFailed                    , "AuthSaveDocFailed" },
        { AuthBadJwtBuilder                    , "AuthBadJwtBuilder" },
        { AuthBadJwtHeader                     , "AuthBadJwtHeader" },
        { AuthBadJwtSubject                    , "AuthBadJwtSubject" },
        { AuthBadJwtAudience                   , "AuthBadJwtAudience" },
        { AuthBadJwtClaim                      , "AuthBadJwtClaim" },
        { AuthBadJwtExpiration                 , "AuthBadJwtExpiration" },
        { AuthJwtSignFailed                    , "AuthJwtSignFailed" },
        { AuthJwtCompactFailed                 , "AuthJwtCompactFailed" },
        { AuthBadJwtChallenge                  , "AuthBadJwtChallenge" },
        { AuthGetJwsClaimFailed                , "AuthGetJwsClaimFailed" },
        { AuthGetPresentationFailed            , "AuthGetPresentationFailed" },
        { AuthInvalidPresentation              , "AuthInvalidPresentation" },
        { AuthPresentationEmptyNonce           , "AuthPresentationEmptyNonce" },
        { AuthPresentationBadNonce             , "AuthPresentationBadNonce" },
        { AuthPresentationEmptyRealm           , "AuthPresentationEmptyRealm" },
        { AuthPresentationBadRealm             , "AuthPresentationBadRealm" },
        { AuthVerifiableCredentialBadCount     , "AuthVerifiableCredentialBadCount" },
        { AuthVerifiableCredentialNotExists    , "AuthVerifiableCredentialNotExists" },
        { AuthVerifiableCredentialInvalid      , "AuthVerifiableCredentialInvalid" },
        { AuthCredentialNotExists              , "AuthCredentialNotExists" },
        { AuthCredentialInvalid                , "AuthCredentialInvalid" },
        { AuthCredentialSerialFailed           , "AuthCredentialSerialFailed" },
        { AuthCredentialParseFailed            , "AuthCredentialParseFailed" },
        { AuthCredentialIdNotExists            , "AuthCredentialIdNotExists" },
        { AuthCredentialBadInstanceId          , "AuthCredentialBadInstanceId" },
        { AuthCredentialPropertyNotExists      , "AuthCredentialPropertyNotExists" },
        { AuthCredentialPropertyAppIdNotExists , "AuthCredentialPropertyAppIdNotExists" },
        { AuthCredentialIssuerNotExists        , "AuthCredentialIssuerNotExists" },
        { AuthCredentialExpirationError        , "AuthCredentialExpirationError" },
        { AuthNonceExpiredError                , "AuthNonceExpiredError" },
        { AuthUpdateOwnerError                 , "AuthUpdateOwnerError" },
        { AuthUpdateUserError                  , "AuthUpdateUserError" },
    };

    auto it = errDespMap.find(errCode);
    if(it != errDespMap.end()) {
        return it->second;
    }

    std::string errMsg = "UnprocessedErrCode";
    if(errCode < StdSystemErrorIndex) { // is std error
        int stdErrVal = StdSystemErrorIndex - errCode;
        auto stdErrCode = std::error_code(stdErrVal, std::generic_category());
        errMsg = stdErrCode.message();
    }

    return errMsg;
}

/***********************************************/
/***** class public function implement  ********/
/***********************************************/

/***********************************************/
/***** class protected function implement  *****/
/***********************************************/


/***********************************************/
/***** class private function implement  *******/
/***********************************************/

} // namespace trinity
