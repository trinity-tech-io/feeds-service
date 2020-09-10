#ifndef _ELASTOS_ERRCODE_HPP_
#define _ELASTOS_ERRCODE_HPP_

#include <string>
#include <functional>

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
	if(errRet > elastos::ErrCode::SourceLineSection) {              \
		errRet += (__LINE__ * elastos::ErrCode::SourceLineSection); \
	}                                                               \

#define GET_ERRCODE(errRet) \
	(errRet - errRet / elastos::ErrCode::SourceLineSection * elastos::ErrCode::SourceLineSection)

    /*** static function and variable ***/
    constexpr static const int UnknownError = -1;
    constexpr static const int UnimplementedError = -2;
    constexpr static const int NotFoundError = -3;
    constexpr static const int InvalidArgument = -5;
    constexpr static const int IOSystemException = -6;
    constexpr static const int NetworkException = -7;
    constexpr static const int PointerReleasedError = -8;
    constexpr static const int DevUUIDError = -9;
    constexpr static const int FileNotExistsError = -10;
    constexpr static const int ConflictWithExpectedError = -12;
	constexpr static const int CreateDirectoryError = -18;
	constexpr static const int SizeOverflowError = -21;

	constexpr static const int AdditivityIndex = -1000;
	constexpr static const int StdSystemErrorIndex = -1000;

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
