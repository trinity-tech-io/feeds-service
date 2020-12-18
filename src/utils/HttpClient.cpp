#include <HttpClient.hpp>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <future>

#if 0
#include <curl/curl.h>
#include <Log.hpp>

#define CHECK_CURL(curl_code) \
	if(curl_code != CURLE_OK) { \
		int errcode = (ErrCode::CurlBaseCode + (-curl_code)); \
		Log::E(Log::Tag::Util, "Failed to call %s, return %d.", FORMAT_METHOD, errcode); \
		return errcode; \
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
int HttpClient::InitGlobal()
{
	std::unique_lock<std::mutex> lock(gMutex);
    if(gIsGlobalInitialized == true) {
        return 0;
    }

	CURLcode curle;
	curle = curl_global_init(CURL_GLOBAL_ALL);
	CHECK_CURL(curle);

	//curl_global_cleanup(); // never called
    gIsGlobalInitialized = true;
	return 0;
}

std::vector<int> HttpClient::MultiGet(std::vector<std::shared_ptr<HttpClient>>& httpClientList)
{
	std::vector<std::future<int>> futureList;
	for(auto& httpClient: httpClientList) {
		auto future = std::async(std::launch::async, std::bind(&HttpClient::syncGet, httpClient));
		futureList.push_back(std::move(future));
	}
	for(auto& future: futureList) {
		future.wait();
	}

	std::vector<int> retList;
	for(auto& future: futureList) {
		retList.push_back(future.get());
	}

	return retList;
}

std::vector<int> HttpClient::MultiPost(std::map<std::shared_ptr<HttpClient>, std::shared_ptr<std::string>>& httpClientList)
{
	std::vector<std::future<int>> futureList;
	for(auto& [httpClient, body]: httpClientList) {
		auto func = static_cast<int (HttpClient::*)(const std::string&)>(&HttpClient::syncPost);
		auto future = std::async(std::launch::async, std::bind(func, httpClient, *body));
		futureList.push_back(std::move(future));
	}
	for(auto& future: futureList) {
		future.wait();
	}

	std::vector<int> retList;
	for(auto& future: futureList) {
		retList.push_back(future.get());
	}

	return retList;
}


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
		return ErrCode::NullArgument;
	}

	if(url.compare(0, std::strlen(SCHEME_HTTP), SCHEME_HTTP) != 0
	&& url.compare(0, std::strlen(SCHEME_HTTPS), SCHEME_HTTPS) != 0) {
		return ErrCode::BadArgument;
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
		return ErrCode::NullArgument;
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
	mRespBody = std::stringstream();

	curle = curl_easy_perform(curlHandlePtr.get());
	CHECK_CURL(curle);

	curle = curl_easy_getinfo(curlHandlePtr.get(), CURLINFO_RESPONSE_CODE, &mRespStatus);
	CHECK_CURL(curle);

	return 0;
}

int HttpClient::syncPost(const int8_t* body, int size)
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
	mRespBody = std::stringstream();

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_POSTFIELDS, body);
	CHECK_CURL(curle);

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_POSTFIELDSIZE, size);
	CHECK_CURL(curle);

	curle = curl_easy_perform(curlHandlePtr.get());
	CHECK_CURL(curle);

	curle = curl_easy_getinfo(curlHandlePtr.get(), CURLINFO_RESPONSE_CODE, &mRespStatus);
	CHECK_CURL(curle);

	return 0;
}

int HttpClient::syncPost(const std::string& body)
{
	int ret = 0;

	ret = syncPost(reinterpret_cast<const int8_t*>(body.data()), (int)body.size());

	return ret;
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
		return ErrCode::NetFailed;
	}

	headers = mRespHeaders;

	return (int)mRespHeaders.size();
}

int HttpClient::getResponseHeader(const std::string& name, HeaderValue& value) const
{
	value.clear();
	if((200 <= mRespStatus && mRespStatus < 300) == false) {
		return ErrCode::NetFailed;
	}
	auto found = mRespHeaders.find(name);
	if(found == mRespHeaders.end()) {
		return ErrCode::NotFound;
	}

	value = found->second;
	auto size = found->second.size();

	return (int)size;
}

int HttpClient::getResponseBody(std::shared_ptr<int8_t>& body)
{
	body.reset();
	if((200 <= mRespStatus && mRespStatus < 300) == false) {
		return ErrCode::NetFailed;
	}

	mRespBody.seekg(0, mRespBody.end);
	auto size = mRespBody.tellg();
	mRespBody.seekg(0, mRespBody.beg);

	body = std::shared_ptr<int8_t>(new int8_t[size], std::default_delete<int8_t[]>());
	mRespBody.read(reinterpret_cast<char*>(body.get()), size);

	return (int)size;
}

int HttpClient::getResponseBody(std::string& body)
{
	body.clear();
	if((200 <= mRespStatus && mRespStatus < 300) == false) {
		return ErrCode::NetFailed;
	}

	body = mRespBody.str();

	mRespBody.seekg(0, mRespBody.end);
	auto size = mRespBody.tellg();
	mRespBody.seekg(0, mRespBody.beg);

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

	httpClient->mRespBody.write(buffer, length);

	return length;
}

size_t HttpClient::CurlReadCallback(char* buffer, size_t size, size_t nitems, void* userdata)
{

	return -1;
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
		return (ErrCode::CurlBaseCode);
	}
	auto curlHandleDeleter = [](CURL* ptr) -> void {
		curl_easy_cleanup(ptr);
	};
	curlHandlePtr = std::shared_ptr<CURL>(curlHandle, curlHandleDeleter);

	curle = curl_easy_setopt(curlHandlePtr.get(), CURLOPT_NOSIGNAL, true);
	CHECK_CURL(curle);

	if(mUrl.empty() == true) {
		return ErrCode::UrlNotExists;
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
            Log::I(Log::Tag::Util, "HttpClient::makeCurl() header=%s", header.c_str());
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
		return ErrCode::NullArgument;
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

#endif