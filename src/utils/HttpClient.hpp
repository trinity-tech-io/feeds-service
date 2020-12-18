#ifndef _HTTP_CLIENT_HPP_
#define _HTTP_CLIENT_HPP_

#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

struct curl_slist;

namespace trinity {

class HttpClient {
public:
	/*** type define ***/
	class ErrCode {
	public:
		static constexpr int Unknown        = -1;
		static constexpr int NotFound       = -2;
		static constexpr int NullArgument   = -3;
		static constexpr int BadArgument    = -4;
		static constexpr int UrlNotExists   = -5;
		static constexpr int UserCanceled   = -6;
		static constexpr int IOFailed       = -7;
		static constexpr int NetFailed      = -8;
		static constexpr int CurlBaseCode   = -100;
	private:
		explicit ErrCode() = delete;
		virtual ~ErrCode() = delete;
	};

	using HeaderValue = std::vector<std::string>;
	using HeaderMap   = std::map<std::string, HeaderValue>;

	/*** static function and variable ***/
	static int InitGlobal();
	static std::vector<int> MultiGet(std::vector<std::shared_ptr<HttpClient>>& httpClientList);
	static std::vector<int> MultiPost(std::map<std::shared_ptr<HttpClient>, std::shared_ptr<std::string>>& httpClientList);

  /*** class function and variable ***/
	explicit HttpClient();
	virtual ~HttpClient();

	int url(const std::string& url);
	int addHeader(const std::string& name, const std::string& value);
	int setHeader(const std::string& name, const std::string& value);
	int setConnectTimeout(unsigned long milliSecond);

	int syncGet();
	int syncPost(const int8_t* body, int size);
	int syncPost(const std::string& body);

	void cancel();

	int getResponseStatus() const;
	int getResponseReason(std::string& message) const;
	int getResponseHeaders(HeaderMap& headers) const;
	int getResponseHeader(const std::string& name, HeaderValue& value) const;
	int getResponseBody(std::shared_ptr<int8_t>& body);
	int getResponseBody(std::string& body);

protected:
  /*** type define ***/

  /*** static function and variable ***/

  /*** class function and variable ***/

private:
  /*** type define ***/
	static constexpr const char* SCHEME_HTTP  = "http://";
	static constexpr const char* SCHEME_HTTPS = "https://";

    static std::mutex gMutex;
    static bool gIsGlobalInitialized;

  /*** static function and variable ***/
	static size_t CurlHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata);
	static size_t CurlWriteCallback(char* buffer, size_t size, size_t nitems, void* userdata);
	static size_t CurlReadCallback(char* buffer, size_t size, size_t nitems, void* userdata);
	static int CurlProgressCallback(void *userdata, double dltotal, double dlnow, double ultotal, double ulnow);

	/*** class function and variable ***/
	int makeCurl(std::shared_ptr<void>& curlHandlePtr, std::shared_ptr<struct curl_slist>& curlHeadersPtr) const;
	int addHeader(HeaderMap& headers,
				  const std::string& name, const std::string& value) const;

	bool mCancelFlag;

	std::string mUrl;
	long mConnectTimeoutMS;
	HeaderMap mReqHeaders;

	int mRespStatus;
	std::string mRespReason;
	HeaderMap mRespHeaders;
	std::stringstream mRespBody;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _HTTP_CLIENT_HPP_ */

