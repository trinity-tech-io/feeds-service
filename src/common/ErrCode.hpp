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
    constexpr static const int UnknownError = -100;
    constexpr static const int UnimplementedError = -101;
    constexpr static const int OutOfMemoryError = -102;
    constexpr static const int NotFoundError = -103;
    constexpr static const int InvalidArgument = -104;
    constexpr static const int IOSystemException = -105;
    constexpr static const int NetworkException = -106;
    constexpr static const int PointerReleasedError = -107;
    constexpr static const int DevUUIDError = -108;
    constexpr static const int FileNotExistsError = -109;
    constexpr static const int ConflictWithExpectedError = -110;
	constexpr static const int CreateDirectoryError = -111;
	constexpr static const int SizeOverflowError = -112;

	constexpr static const int CarrierSessionInitFailed = -120;
	constexpr static const int CarrierSessionCreateFailed = -121;
	constexpr static const int CarrierSessionAddStreamFailed = -122;
	constexpr static const int CarrierSessionTimeoutError = -123;
	constexpr static const int CarrierSessionReplyFailed = -124;
	constexpr static const int CarrierSessionStartFailed = -125;
	constexpr static const int CarrierSessionBadStatus = -126;
	constexpr static const int CarrierSessionDataNotEnough = -127;
	constexpr static const int CarrierSessionUnsuppertedVersion = -128;
	constexpr static const int CarrierSessionReleasedError = -129;

	// constexpr static const int CarrierIndex = -1000;
	constexpr static const int StdSystemErrorIndex = -2000;

	constexpr static const int SourceLineSection = -1000000;

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
