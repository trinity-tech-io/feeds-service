#include "MassData.hpp"

#include <cstring>
#include <fstream>
#include <ErrCode.hpp>
#include <Log.hpp>
#include <Random.hpp>
#include <SafePtr.hpp>
#include <crystal.h>

namespace trinity {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */
MassData::MassData(const std::filesystem::path& massDataDir)
    : massDataDir(massDataDir)
{
    using namespace std::placeholders;
    std::map<const char*, Handler> cmdHandleMap {
        {Method::SetBinary, {std::bind(&MassData::onSetBinary, this, _1, _2), Accessible::Owner}},
        {Method::GetBinary, {std::bind(&MassData::onGetBinary, this, _1, _2), Accessible::Member}},
    };

    setHandleMap(cmdHandleMap);
}

MassData::~MassData()
{
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */
int MassData::saveBinData(const std::filesystem::path &massDataDir,
                          const std::shared_ptr<Req>& req,
                          std::shared_ptr<Resp> &resp,
                          const std::filesystem::path &contentFilePath)
{
    auto setBinReq = std::reinterpret_pointer_cast<SetBinaryReq>(req);
    Log::D(Log::TAG, "Request params:");
    Log::D(Log::TAG, "    access_token: %s", setBinReq->params.tk);
    Log::D(Log::TAG, "    key: %s", setBinReq->params.key);
    Log::D(Log::TAG, "    algo: %s", setBinReq->params.algo);
    Log::D(Log::TAG, "    checksum: %s", setBinReq->params.checksum);
    Log::D(Log::TAG, "    content_size: %d", setBinReq->params.content_sz);

    auto setBinResp = std::make_shared<SetBinaryResp>();
    setBinResp->tsx_id = setBinReq->tsx_id;

    if(std::strcmp(setBinReq->params.algo, "None") != 0) {
        CHECK_ERROR(ErrCode::CmdUnsupportedAlgo);
    }

    auto keyPath = massDataDir / setBinReq->params.key;
    Log::V(Log::TAG, "Resave %s to %s.", contentFilePath.c_str(), keyPath.c_str());
    std::error_code ec;
    std::filesystem::rename(contentFilePath, keyPath, ec); // noexcept
    if(ec.value() != 0) {
        CHECK_ERROR(ErrCode::StdSystemErrorIndex + (-ec.value()));
    }

    setBinResp->result.key = setBinReq->params.key;
    Log::D(Log::TAG, "Response result:");
    Log::D(Log::TAG, "    key: %s", setBinResp->result.key);

    resp = std::reinterpret_pointer_cast<Resp>(setBinResp);

    return 0;
}

int MassData::loadBinData(const std::filesystem::path &massDataDir,
                          const std::shared_ptr<Req>& req,
                          std::shared_ptr<Resp> &resp,
                          std::filesystem::path &contentFilePath)
{
    auto getBinReq = std::reinterpret_pointer_cast<GetBinaryReq>(req);
    Log::D(Log::TAG, "    access_token: %s", getBinReq->params.tk);
    Log::D(Log::TAG, "    key: %s", getBinReq->params.key);

    auto getBinResp = std::make_shared<GetBinaryResp>();
    getBinResp->tsx_id = getBinReq->tsx_id;

    auto keyPath = massDataDir / getBinReq->params.key;
    Log::V(Log::TAG, "Try to load %s.", keyPath.c_str());
    if(std::filesystem::exists(keyPath) == false) {
        CHECK_ERROR(ErrCode::FileNotExistsError);
    }

    contentFilePath = keyPath;

    getBinResp->result.key = getBinReq->params.key;
    getBinResp->result.algo = const_cast<char*>("None");
    getBinResp->result.checksum = const_cast<char*>("");
    getBinResp->result.content = nullptr;
    getBinResp->result.content_sz = 0;
    Log::D(Log::TAG, "Response result:");
    Log::D(Log::TAG, "    key: %s", getBinResp->result.key);
    Log::D(Log::TAG, "    algo: %s", getBinResp->result.algo);
    Log::D(Log::TAG, "    checksum: %s", getBinResp->result.checksum);
    Log::D(Log::TAG, "    content_path: %s", contentFilePath.c_str());

    resp = std::reinterpret_pointer_cast<Resp>(getBinResp);

    return 0;
}

/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
int MassData::onSetBinary(std::shared_ptr<Req> req,
                          std::shared_ptr<Resp> &resp)
{
    auto setBinReq = std::reinterpret_pointer_cast<SetBinaryReq>(req);

    auto massDataCacheDir = massDataDir/ MassDataCacheDirName;
    if(std::filesystem::exists(massDataCacheDir) == false) {
        bool created = std::filesystem::create_directories(massDataCacheDir);
        CHECK_ASSERT(created, ErrCode::FileNotExistsError);
    }
    auto massDataCacheFilePath = massDataCacheDir / (MassDataCacheName + std::to_string(Random::Gen<uint32_t>()));

    std::fstream massDataCacheStream;
    massDataCacheStream.open(massDataCacheFilePath,
                             std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
    massDataCacheStream.write((char*)setBinReq->params.content, setBinReq->params.content_sz);
    massDataCacheStream.flush();
    massDataCacheStream.close();

    int ret = saveBinData(massDataDir, req, resp, massDataCacheFilePath);
    std::filesystem::remove(massDataCacheFilePath);
    CHECK_ERROR(ret);

    return 0;
}

int MassData::onGetBinary(std::shared_ptr<Req> req,
                          std::shared_ptr<Resp> &resp)
{
    constexpr const int MAX_CONTENT_SIZE = 4 * 1024 * 1024; // 4MB

    std::filesystem::path massDataFilePath;
    int ret = loadBinData(massDataDir, req, resp, massDataFilePath);
    CHECK_ERROR(ret);

    auto getBinResp = std::reinterpret_pointer_cast<GetBinaryResp>(resp);
    getBinResp->result.content_sz = std::filesystem::file_size(massDataFilePath);
    CHECK_ASSERT(getBinResp->result.content_sz <= MAX_CONTENT_SIZE, ErrCode::SizeOverflowError);

    getBinResp->result.content = rc_zalloc(getBinResp->result.content_sz, NULL);
    CHECK_ASSERT(getBinResp->result.content, ErrCode::OutOfMemoryError);

    std::fstream massDataCacheStream;
    massDataCacheStream.open(massDataFilePath,
                             std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
    massDataCacheStream.seekg(0);
    massDataCacheStream.read((char*)getBinResp->result.content, getBinResp->result.content_sz);
    massDataCacheStream.close();

    return 0;
}

} // namespace trinity
