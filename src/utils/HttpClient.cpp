#include <HttpClient.hpp>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <future>

#include <curl/curl.h>
#include <ErrCode.hpp>
#include <Log.hpp>

#define CHECK_CURL(curl_code) \
	if(curl_code != CURLE_OK) { \
		Log::E(Log::Tag::Util, "Failed to call curl, curl error desc is: %s(%d).", curl_easy_strerror(curl_code), curl_code); \
		CHECK_ERROR(ErrCode::HttpClientCurlErrStart + (-curl_code)); \
	}

namespace trinity {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */
std::mutex HttpClient::gMutex {};
bool HttpClient::gIsGlobalInitialized = false;

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */
HttpClient::HttpClient()
	: mCancelFlag(false)
	, mUrl()
	, mConnectTimeoutMS(10000)
	, mReqHeaders()
	, mRespStatus(-1)
	, mRespReason()
	, mRespHeaders()
	, mRespBody()
{
}

HttpClient::~HttpClient()
{
}

int HttpClient::url(const std::string& url)
{
	if(url.empty() == true) {
		return ErrCode::HttpClientNullArgument;
	}

	if(url.compare(0, std::strlen(SCHEME_HTTP), SCHEME_HTTP) != 0
	&& url.compare(0, std::strlen(SCHEME_HTTPS), SCHEME_HTTPS) != 0) {
		return ErrCode::HttpClientBadArgument;
	}

	mUrl = url;

	return 0;
}

int HttpClient::addHeader(const std::string& name, const std::string& value)
{
	int ret = 0;

	ret = addHeader(mReqHeaders, name, value);

	return ret;
}

int HttpClient::setHeader(const std::string& name, const std::string& value)
{
	int ret = 0;
	if(name.empty() == true) {
		return ErrCode::HttpClientNullArgument;
	}

	auto found = mReqHeaders.find(name);
	if(found != mReqHeaders.end()) {
		found->second.clear();
	}

	ret = addHeader(name, value);

	return ret;
}

int HttpClient::setConnectTimeout(unsigned long milliSecond)
{
	mConnectTimeoutMS = milliSecond;

	return 0;
}

int HttpClient::setResponseBody(std::shared_ptr<std::ostream> body)
{
	mRespBody = body;

	return 0;
}

int HttpClient::syncGet()
{
	int ret;
	CURLcode curle;
	std::shared_ptr<CURL> curlHandlePtr;
	std::shared_ptr<struct curl_slist> curlHeadersPtr;

	ret = makeCurl(curlHandlePtr, curlHeadersPtr);
	if(ret < 0) {
		return ret;
	}

	mRespStatus = -1;
	mRespReason.clear();
	mRespHeaders.clear();

	curle = curl_easy_perform(curlHandlePtr.get());
	CHECK_CURL(curle);

	curle = curl_easy_getinfo(curlHandlePtr.get(), CURLINFO_RESPONSE_CODE, &mRespStatus);
	CHECK_CURL(curle);

	return 0;
}

int HttpClient::syncPost(std::shared_ptr<std::istream> body)
{
	int ret;
	CURLcode curle;
	std::shared_ptr<CURL> curlHandlePtr;
	std::shared_ptr<struct curl_slist> curlHeadersPtr;

	ret = makeCurl(curlHandlePtr, curlHeadersPtr);
	CHECK_ERROR(ret);

	mReqBody = body;

	mRespStatus = -1;
	mRespReason.clear();
	mRespHeaders.clear();

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_POST, 1L);
	CHECK_CURL(curle);

	curle = curl_easy_perform(curlHandlePtr.get());
	CHECK_CURL(curle);

	curle = curl_easy_getinfo(curlHandlePtr.get(), CURLINFO_RESPONSE_CODE, &mRespStatus);
	CHECK_CURL(curle);

	return 0;
}

void HttpClient::asyncGet(std::function<void(int)> callback)
{
	std::ignore = std::async(std::launch::async, [=] {
		int ret = this->syncGet();
		callback(ret);
	});
}

void HttpClient::asyncPost(std::shared_ptr<std::istream> body, std::function<void(int)> callback)
{
	std::ignore = std::async(std::launch::async, [=] {
		int ret = this->syncPost(body);
		callback(ret);
	});
}

void HttpClient::cancel()
{
	Log::I(Log::Tag::Util, "%s url:%s", FORMAT_METHOD, mUrl.c_str());
	mCancelFlag = true;
}

int HttpClient::getResponseStatus() const
{
	return mRespStatus;
}

int HttpClient::getResponseHeaders(HeaderMap& headers) const
{
	headers.clear();
	if((200 <= mRespStatus && mRespStatus < 300) == false) {
		return ErrCode::HttpClientNetFailed;
	}

	headers = mRespHeaders;

	return (int)mRespHeaders.size();
}

int HttpClient::getResponseHeader(const std::string& name, HeaderValue& value) const
{
	value.clear();
	if((200 <= mRespStatus && mRespStatus < 300) == false) {
		return ErrCode::HttpClientNetFailed;
	}
	auto found = mRespHeaders.find(name);
	if(found == mRespHeaders.end()) {
		return ErrCode::HttpClientHeaderNotFound;
	}

	value = found->second;
	auto size = found->second.size();

	return (int)size;
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
std::string trim(const std::string &s)
{
   auto wsfront=std::find_if_not(s.begin(),s.end(),[](int c){return std::isspace(c);});
   auto wsback=std::find_if_not(s.rbegin(),s.rend(),[](int c){return std::isspace(c);}).base();
   return (wsback<=wsfront ? std::string() : std::string(wsfront,wsback));
}

size_t HttpClient::CurlHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata)
{
	HttpClient* httpClient = static_cast<HttpClient*>(userdata);
	std::string header(buffer);
	size_t length = size * nitems;

	auto pos = header.find(':');
	if(pos != std::string::npos) {
		std::string name = header.substr(0, pos);
		std::string value = header.substr(pos + 1);
		name = trim(name);
		value = trim(value);

		httpClient->addHeader(httpClient->mRespHeaders, name, value);
	}

	return length;
}

size_t HttpClient::CurlWriteCallback(char* buffer, size_t size, size_t nitems, void* userdata)
{
	HttpClient* httpClient = static_cast<HttpClient*>(userdata);
	size_t length = size * nitems;

	if(httpClient->mRespBody.get() != nullptr) {
		httpClient->mRespBody->write(buffer, length);
	}

	return length;
}

size_t HttpClient::CurlReadCallback(char* buffer, size_t size, size_t nitems, void* userdata)
{
	HttpClient* httpClient = static_cast<HttpClient*>(userdata);
	size_t length = size * nitems;

	if(httpClient->mReqBody.get() != nullptr) {
		httpClient->mReqBody->read(buffer, length);
	}

	return length;
}

int HttpClient::CurlProgressCallback(void *userdata, double dltotal, double dlnow, double ultotal, double ulnow)
{
	HttpClient* httpClient = static_cast<HttpClient*>(userdata);
//	Log::I(Log::Tag::Util, "HttpClient::CurlProgressCallback() mCancelFlag=%d.", httpClient->mCancelFlag);
	if(httpClient->mCancelFlag == true) {
		Log::I(Log::Tag::Util, "HttpClient::CurlProgressCallback() cancel by user.");
		return -1;
	}

	return 0;
}

int HttpClient::makeCurl(std::shared_ptr<void>& curlHandlePtr, std::shared_ptr<struct curl_slist>& curlHeadersPtr) const
{
	CURLcode curle;

	curlHandlePtr.reset();
	curlHeadersPtr.reset();

	CURL* curlHandle = curl_easy_init();
	if(curlHandle == nullptr) {
		return (ErrCode::HttpClientCurlErrStart);
	}
	auto curlHandleDeleter = [](CURL* ptr) -> void {
		curl_easy_cleanup(ptr);
	};
	curlHandlePtr = std::shared_ptr<CURL>(curlHandle, curlHandleDeleter);

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_NOSIGNAL, true);
	CHECK_CURL(curle);

	if(mUrl.empty() == true) {
		return ErrCode::HttpClientUrlNotExists;
	}
    Log::I(Log::Tag::Util, "HttpClient::makeCurl() url=%s", mUrl.c_str());
	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_URL, mUrl.c_str());
	CHECK_CURL(curle);

	struct curl_slist* curlHeaders = nullptr;
	bool hasAcceptEncoding = false;
	for (auto& [key, val]: mReqHeaders) {
		auto& name = key;
		for(auto& value: val) {
			std::string header = (name + ": " + value);
            Log::D(Log::Tag::Util, "HttpClient::makeCurl() header=%s", header.c_str());
			curlHeaders = curl_slist_append(curlHeaders, header.c_str());
		}

		if(key == "Accept-Encoding") {
			hasAcceptEncoding = true;
		}
	}
	if(hasAcceptEncoding == false) {
		curlHeaders = curl_slist_append(curlHeaders, "Accept-Encoding: identity");
	}
	auto curlHeadersDeleter = [](struct curl_slist* ptr) -> void {
		curl_slist_free_all(ptr);
	};
	curlHeadersPtr = std::shared_ptr<struct curl_slist>(curlHeaders, curlHeadersDeleter);

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_SSL_VERIFYHOST, 0L);
	CHECK_CURL(curle);

    curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_SSL_VERIFYPEER, 0L);
	CHECK_CURL(curle);

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_HTTPHEADER, curlHeadersPtr.get());
	CHECK_CURL(curle);

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_CONNECTTIMEOUT_MS, mConnectTimeoutMS);
	CHECK_CURL(curle);

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_HEADERFUNCTION, CurlHeaderCallback);
	CHECK_CURL(curle);

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_HEADERDATA, this);
	CHECK_CURL(curle);

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_WRITEFUNCTION, CurlWriteCallback);
	CHECK_CURL(curle);
	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_WRITEDATA, this);
	CHECK_CURL(curle);

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_READFUNCTION, CurlReadCallback);
	CHECK_CURL(curle);
	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_READDATA, this);
	CHECK_CURL(curle);

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_NOPROGRESS, 0);
	CHECK_CURL(curle);
	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_PROGRESSFUNCTION, CurlProgressCallback);
	CHECK_CURL(curle);
	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_PROGRESSDATA, this);
	CHECK_CURL(curle);

	return 0;
}

int HttpClient::addHeader(HeaderMap& headers, const std::string& name, const std::string& value) const
{
	if(name.empty() == true) {
		return ErrCode::HttpClientNullArgument;
	}

	auto found = headers.find(name);
	if(found == headers.end()) {
		headers[name] = HeaderValue();
		found = headers.find(name);
	}

	found->second.push_back(value);

	return 0;
}

} // namespace trinity