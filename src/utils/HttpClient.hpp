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
	using HeaderValue = std::vector<std::string>;
	using HeaderMap   = std::map<std::string, HeaderValue>;

	/*** static function and variable ***/
  
  	/*** class function and variable ***/
	explicit HttpClient();
	virtual ~HttpClient();

	int url(const std::string& url);
	int addHeader(const std::string& name, const std::string& value);
	int setHeader(const std::string& name, const std::string& value);
	int setConnectTimeout(unsigned long milliSecond);
	int setResponseBody(std::shared_ptr<std::ostream> body);

	int syncGet();
	int syncPost(std::shared_ptr<std::istream> body);

	void asyncGet(std::function<void(int)> callback);
	void asyncPost(std::shared_ptr<std::istream> body, std::function<void(int)> callback);

	void cancel();

	int getResponseStatus() const;
	int getResponseReason(std::string& message) const;
	int getResponseHeaders(HeaderMap& headers) const;
	int getResponseHeader(const std::string& name, HeaderValue& value) const;

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
	std::shared_ptr<std::istream> mReqBody;

	int mRespStatus;
	std::string mRespReason;
	HeaderMap mRespHeaders;
	std::shared_ptr<std::ostream> mRespBody;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _HTTP_CLIENT_HPP_ */

