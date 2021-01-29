#ifndef _FEEDS_ERRCODE_HPP_
#define _FEEDS_ERRCODE_HPP_

#include <string>
#include <functional>
#include <Log.hpp>

extern "C" {
#include "err.h"
}

namespace trinity {

class ErrCode {
public:
    /*** type define ***/
#define CHECK_ERROR(errCode) \
	if((errCode) < 0) { \
	    int errRet = (errCode); \
		APPEND_SRCLINE(errRet); \
		Log::E(Log::Tag::Err, "Failed to call %s in line %d, return %s(%d).", FORMAT_METHOD, __LINE__, ErrCode::ToString(errRet).c_str(), errRet); \
		return errRet; \
	}

#define CHECK_RETVAL(errCode) \
	if(errCode < 0) { \
	    int errRet = errCode; \
		APPEND_SRCLINE(errRet); \
		Log::E(Log::Tag::Err, "Failed to call %s in line %d, return %s(%d).", FORMAT_METHOD, __LINE__, ErrCode::ToString(errRet).c_str(), errRet); \
        return; \
	}

#define CHECK_ASSERT(expr, errCode) \
	if(!(expr)) { \
        CHECK_ERROR(errCode); \
	}

#define CHECK_AND_RETDEF(errCode, def) \
	if(errCode < 0) { \
	    int errRet = errCode; \
		APPEND_SRCLINE(errRet); \
		Log::E(Log::Tag::Err, "Failed to call %s in line %d, return %s(%d).", FORMAT_METHOD, __LINE__, ErrCode::ToString(errRet).c_str(), errRet); \
		return def; \
	}

#define CHECK_AND_NOTIFY_ERROR(errCode) \
	if(errCode < 0) { \
	    int errRet = errCode; \
		APPEND_SRCLINE(errRet); \
		trinity::ErrCode::SetError(errRet, std::string(FORMAT_METHOD) + " line:" + std::to_string(__LINE__)); \
	} \
    CHECK_ERROR(errCode) \

#define CHECK_AND_NOTIFY_RETVAL(errCode) \
	if(errCode < 0) { \
	    int errRet = errCode; \
		APPEND_SRCLINE(errRet); \
		trinity::ErrCode::SetError(errRet, std::string(FORMAT_METHOD) + " line:" + std::to_string(__LINE__)); \
	} \
    CHECK_RETVAL(errCode) \

#define APPEND_SRCLINE(errRet) \
	if(errRet < trinity::ErrCode::SourceLineSection) {              \
		errRet += (__LINE__ * trinity::ErrCode::SourceLineSection); \
	}                                                               \

#define GET_ERRCODE(errRet) \
	(errRet - errRet / trinity::ErrCode::SourceLineSection * trinity::ErrCode::SourceLineSection)

    /*** static function and variable ***/
    constexpr static const int ReservedError                    = -100;
    constexpr static const int UnknownError                     = -101;
    constexpr static const int UnimplementedError               = -102;
    constexpr static const int NotFoundError                    = -103;
    constexpr static const int InvalidArgument                  = -104;
    constexpr static const int PointerReleasedError             = -105;
    constexpr static const int DevUUIDError                     = -106;
    constexpr static const int FileNotExistsError               = -107;
    constexpr static const int CreateDirectoryError             = -108;
    constexpr static const int SizeOverflowError                = -109;
    constexpr static const int StdSystemError                   = -110;
    constexpr static const int OutOfMemoryError                 = -111;
    constexpr static const int CompletelyFinishedNotify         = -112;
    constexpr static const int DirectoryNotExistsError          = -113;
    constexpr static const int BadFileSize                      = -114;
    constexpr static const int BadFileMd5                       = -115;
    constexpr static const int ExecSystemCommendFailed          = -116;

    constexpr static const int DidNotReady                      = -120;
    constexpr static const int InvalidAccessToken               = -121;
    constexpr static const int NotAuthorizedError               = -122;
    constexpr static const int InvalidParams                    = -123;

    constexpr static const int CarrierSessionInitFailed         = -130;
    constexpr static const int CarrierSessionConnectFailed      = -131;
    constexpr static const int CarrierSessionCreateFailed       = -132;
    constexpr static const int CarrierSessionAddStreamFailed    = -133;
    constexpr static const int CarrierSessionTimeoutError       = -134;
    constexpr static const int CarrierSessionReplyFailed        = -135;
    constexpr static const int CarrierSessionStartFailed        = -136;
    constexpr static const int CarrierSessionBadStatus          = -137;
    constexpr static const int CarrierSessionDataNotEnough      = -138;
    constexpr static const int CarrierSessionUnsuppertedVersion = -139;
    constexpr static const int CarrierSessionReleasedError      = -140;
    constexpr static const int CarrierSessionSendFailed         = -141;
    constexpr static const int CarrierSessionErrorExists        = -142;

    constexpr static const int CmdUnknownReqFailed              = -150;
    constexpr static const int CmdUnmarshalReqFailed            = -151;
    constexpr static const int CmdMarshalRespFailed             = -152;
    constexpr static const int CmdUnsupportedVersion 	        = -153;
    constexpr static const int CmdUnsupportedAlgo               = -154;
    constexpr static const int CmdUnknownRespFailed             = -155;
    constexpr static const int CmdSendFailed                    = -155;

    constexpr static const int AuthBadDidDoc                       = -160;
    constexpr static const int AuthDidDocInvlid                    = -161;
    constexpr static const int AuthBadDid                          = -162;
    constexpr static const int AuthBadDidString                    = -163;
    constexpr static const int AuthSaveDocFailed                   = -164;
    constexpr static const int AuthBadJwtBuilder                   = -165;
    constexpr static const int AuthBadJwtHeader                    = -166;
    constexpr static const int AuthBadJwtSubject                   = -167;
    constexpr static const int AuthBadJwtAudience                  = -168;
    constexpr static const int AuthBadJwtClaim                     = -169;
    constexpr static const int AuthBadJwtExpiration                = -170;
    constexpr static const int AuthJwtSignFailed                   = -171;
    constexpr static const int AuthJwtCompactFailed                = -172;
    constexpr static const int AuthBadJwtChallenge                 = -173;
    constexpr static const int AuthGetJwsClaimFailed               = -174;
    constexpr static const int AuthGetPresentationFailed           = -175;
    constexpr static const int AuthInvalidPresentation             = -176;
    constexpr static const int AuthPresentationEmptyNonce          = -177;
    constexpr static const int AuthPresentationBadNonce            = -178;
    constexpr static const int AuthPresentationEmptyRealm          = -179;
    constexpr static const int AuthPresentationBadRealm            = -180;
    constexpr static const int AuthVerifiableCredentialBadCount    = -181;
    constexpr static const int AuthVerifiableCredentialNotExists   = -182;
    constexpr static const int AuthVerifiableCredentialInvalid     = -183;
    constexpr static const int AuthCredentialNotExists             = -184;
    constexpr static const int AuthCredentialInvalid               = -185;
    constexpr static const int AuthCredentialSerialFailed          = -186;
    constexpr static const int AuthCredentialParseFailed           = -187;
    constexpr static const int AuthCredentialIdNotExists           = -188;
    constexpr static const int AuthCredentialBadInstanceId         = -189;
    constexpr static const int AuthCredentialPropertyNotExists     = -190;
    constexpr static const int AuthCredentialPropertyAppIdNotExists = -191;
    constexpr static const int AuthCredentialIssuerNotExists       = -192;
    constexpr static const int AuthCredentialExpirationError       = -193;
    constexpr static const int AuthNonceExpiredError               = -194;
    constexpr static const int AuthUpdateOwnerError                = -195;
    constexpr static const int AuthUpdateUserError                 = -196;
    
    constexpr static const int DBOpenFailed                        = -200;
    constexpr static const int DBInitFailed                        = -201;
    constexpr static const int DBException                         = -202;

    constexpr static const int MsgPackCreateFailed                 = -230;
    constexpr static const int MsgPackInvalidStruct                = -231;
    constexpr static const int MsgPackInvalidValue                 = -232;
    constexpr static const int MsgPackParseFailed                  = -233;
    constexpr static const int MsgPackUnprocessedMethod            = -234;
    constexpr static const int MsgPackUnknownRequest               = -235;

    constexpr static const int RpcUnimplementedError               = -250;

    constexpr static const int HttpClientCurlErrStart              = -300;
    constexpr static const int HttpClientCurlErrEnd                = -450;
    constexpr static const int HttpClientUnknownError              = -451;
    constexpr static const int HttpClientHeaderNotFound            = -452;
    constexpr static const int HttpClientNullArgument              = -453;
    constexpr static const int HttpClientBadArgument               = -454;
    constexpr static const int HttpClientUrlNotExists              = -455;
    constexpr static const int HttpClientUserCanceled              = -456;
    constexpr static const int HttpClientIOFailed                  = -457;
    constexpr static const int HttpClientNetFailed                 = -458;

    constexpr static const int AutoUpdateAlreadyNewest             = -480;
    constexpr static const int AutoUpdateUnsuppertProduct          = -481;
    constexpr static const int AutoUpdateBadRuntimeDir             = -482;
    constexpr static const int AutoUpdateMoveTarballFailed         = -483;
    constexpr static const int AutoUpdateBadTarball                = -484;
    constexpr static const int AutoUpdateKillRuntimeFailed         = -485;
    constexpr static const int AutoUpdateStartRuntimeFailed        = -486;
    constexpr static const int AutoUpdateReadLinkFailed            = -487;
    constexpr static const int AutoUpdateRemoveLinkFailed          = -488;
    constexpr static const int AutoUpdateMakeLinkFailed            = -489;

    constexpr static const int CloudDriveMakeDirFailed             = -500;
    constexpr static const int CloudDriveTouchFileFailed           = -501;
    constexpr static const int CloudDriveUploadFileFailed          = -502;
    constexpr static const int CloudDriveDownloadFileFailed        = -503;
    constexpr static const int CloudDriveDeleteFileFailed          = -504;

    // constexpr static const int CarrierIndex                  = -1000;
    constexpr static const int StdSystemErrorIndex              = -2000;

    constexpr static const int SourceLineSection                = -1000000;

	static void SetErrorListener(std::function<void(int, const std::string&, const std::string&)> listener);
	static void SetError(int errCode, const std::string& ext);
    static std::string ToString(int errCode);

    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/
	static std::function<void(int, const std::string&, const std::string&)> sErrorListener;

    /*** class function and variable ***/
    explicit ErrCode() = delete;
    virtual ~ErrCode() = delete;

}; // class ErrCode

} // namespace trinity

#endif /* _FEEDS_ERRCODE_HPP_ */
