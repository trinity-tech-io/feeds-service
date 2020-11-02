#ifndef _ELASTOS_ERRCODE_HPP_
#define _ELASTOS_ERRCODE_HPP_

#include <string>
#include <functional>

#include "err.h"

namespace elastos {

class ErrCode {
public:
    /*** type define ***/
#define CHECK_ERROR(errCode) \
	if((errCode) < 0) { \
	    int errRet = (errCode); \
		APPEND_SRCLINE(errRet); \
		Log::E(Log::TAG, "Failed to call %s in line %d, return %d.", FORMAT_METHOD, __LINE__, errRet); \
		return errRet; \
	}

#define CHECK_RETVAL(errCode) \
	if(errCode < 0) { \
	    int errRet = errCode; \
		APPEND_SRCLINE(errRet); \
		Log::E(Log::TAG, "Failed to call %s in line %d, return %d.", FORMAT_METHOD, __LINE__, errRet); \
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
		Log::E(Log::TAG, "Failed to call %s in line %d, return %d.", FORMAT_METHOD, __LINE__, errRet); \
		return def; \
	}

#define CHECK_AND_NOTIFY_ERROR(errCode) \
	if(errCode < 0) { \
	    int errRet = errCode; \
		APPEND_SRCLINE(errRet); \
		elastos::ErrCode::SetError(errRet, std::string(FORMAT_METHOD) + " line:" + std::to_string(__LINE__)); \
	} \
    CHECK_ERROR(errCode) \

#define CHECK_AND_NOTIFY_RETVAL(errCode) \
	if(errCode < 0) { \
	    int errRet = errCode; \
		APPEND_SRCLINE(errRet); \
		elastos::ErrCode::SetError(errRet, std::string(FORMAT_METHOD) + " line:" + std::to_string(__LINE__)); \
	} \
    CHECK_RETVAL(errCode) \

#define APPEND_SRCLINE(errRet) \
	if(errRet < elastos::ErrCode::SourceLineSection) {              \
		errRet += (__LINE__ * elastos::ErrCode::SourceLineSection); \
	}                                                               \

#define GET_ERRCODE(errRet) \
	(errRet - errRet / elastos::ErrCode::SourceLineSection * elastos::ErrCode::SourceLineSection)

    /*** static function and variable ***/
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

    constexpr static const int DidNotReady                      = -120;
    constexpr static const int InvalidAccessToken               = -121;
    constexpr static const int NotAuthorizedError               = -122;

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

    constexpr static const int MassDataUnknownReqFailed         = -150;
    constexpr static const int MassDataUnmarshalReqFailed       = -151;
    constexpr static const int MassDataMarshalRespFailed        = -152;
    constexpr static const int MassDataUnsupportedVersion 	    = -153;
    constexpr static const int MassDataUnsupportedAlgo          = -154;

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

} // namespace elastos

#endif /* _ELASTOS_ERRCODE_HPP_ */
