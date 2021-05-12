#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#endif

#include "SessionParser.hpp"

#include <Random.hpp>
#include <SafePtr.hpp>
#include <DateTime.hpp>

#ifdef _MSC_VER
#define typeof(v)        decltype(v)
#endif

#if defined(__linux__) and not defined(__ANDROID__)
#include <arpa/inet.h>
#if __BIG_ENDIAN__
# define htonll(x) (x)
# define ntohll(x) (x)
#else
# define htonll(x) ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32)
# define ntohll(x) ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32)
#endif
#endif

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
void SessionParser::config(const std::filesystem::path& cacheDir)
{
    this->bodyCacheDir = cacheDir;
}

int SessionParser::unpack(const std::vector<uint8_t>& data,
                          std::shared_ptr<OnUnpackedListener> listener)
{
    auto dataPos = 0;

    do {
        int ret = unpackProtocol(data, dataPos);
        if(ret == ErrCode::CarrierSessionDataNotEnough) {
            return 0; // Ignore to dump backtrace
        }
        CHECK_ERROR(ret);
        dataPos += ret;

        ret = unpackBodyData(data, dataPos, listener);
        CHECK_ERROR(ret);
        dataPos += ret;
    } while(dataPos < data.size());

    return 0;
}

 int SessionParser::pack(std::vector<uint8_t>& data,
                         const std::vector<uint8_t>& headData,
                         const std::filesystem::path& bodyPath)
{
    data.clear();

    uint64_t bodySize = 0;
    if(bodyPath.empty() == false
    && std::filesystem::exists(bodyPath) == true) {
        bodySize = std::filesystem::file_size(bodyPath);
    }

    auto result = std::make_unique<Protocol>();
    result->info.magicNumber = Protocol::MagicNumber;
    result->info.version = Protocol::Version_01_00_00;
    result->info.headSize = headData.size();
    result->info.bodySize = bodySize;

    uint8_t* dataPtr = nullptr;

    auto netOrderMagicNum = hton(result->info.magicNumber);
    dataPtr = reinterpret_cast<uint8_t*>(&netOrderMagicNum);
    data.insert(data.end(), dataPtr, dataPtr + sizeof(result->info.magicNumber));

    auto netOrderVersion = hton(result->info.version);
    dataPtr = reinterpret_cast<uint8_t*>(&netOrderVersion);
    data.insert(data.end(), dataPtr, dataPtr + sizeof(result->info.version));

    auto netOrderHeadSize = hton(result->info.headSize);
    dataPtr = reinterpret_cast<uint8_t*>(&netOrderHeadSize);
    data.insert(data.end(), dataPtr, dataPtr + sizeof(result->info.headSize));

    auto netOrderBodySize = hton(result->info.bodySize);
    dataPtr = reinterpret_cast<uint8_t*>(&netOrderBodySize);
    data.insert(data.end(), dataPtr, dataPtr + sizeof(result->info.bodySize));

    return data.size();
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
int SessionParser::unpackProtocol(const std::vector<uint8_t>& data, int offset)
{
     // protocal info has been parsed, value data is body payload, return directly.
    if(protocol != nullptr
    && protocol->info.headSize == protocol->payload->headData.size()) {
        // Log::D(Log::Tag::Msg, "Protocol has been parsed.");
        return 0;
    }

    auto cachingDataPrevSize = cachingData.size();
    cachingData.insert(cachingData.end(), data.begin() + offset, data.end());

    if(protocol == nullptr) {
        // find first magic number and remove garbage data.
        auto searchMagicNum = hton(Protocol::MagicNumber);
        int garbageIdx;
        for(garbageIdx = 0; garbageIdx <= static_cast<int>(cachingData.size() - sizeof(Protocol::Info)); garbageIdx++) {
            if(searchMagicNum == *((typeof(Protocol::Info::magicNumber)*)(cachingData.data() + garbageIdx))) {
                break;
            }
        }
        if(garbageIdx > 0) {
            Log::W(Log::Tag::Msg, "Remove garbage size %d", garbageIdx);
            cachingData.erase(cachingData.begin(), cachingData.begin() + garbageIdx);
            cachingDataPrevSize -= garbageIdx; // recompute valid data of previous cache.
        }

        // return and parse next time if data is not enough to parse info.
        if(cachingData.size() < sizeof(Protocol::Info)) {
            Log::D(Log::Tag::Msg, "Protocol info data is not enough.");
            return ErrCode::CarrierSessionDataNotEnough;
        }

        if(std::filesystem::exists(bodyCacheDir) == false) {
            bool created = std::filesystem::create_directories(bodyCacheDir);
            CHECK_ASSERT(created, ErrCode::FileNotExistsError);
        }
        auto bodyPath = bodyCacheDir / (BodyCacheName + std::to_string(Random::Gen<uint32_t>()));
        protocol = std::make_unique<Protocol>();
        protocol->payload = std::make_unique<Protocol::Payload>(bodyPath);

        auto dataPtr = cachingData.data();

        //auto netOrderMagicNum = *(int64_t*)(dataPtr);
        auto netOrderMagicNum = *((typeof(protocol->info.magicNumber)*)(dataPtr));
        protocol->info.magicNumber = ntoh(netOrderMagicNum);
        dataPtr += sizeof(protocol->info.magicNumber);

        auto netOrderVersion = *((typeof(protocol->info.version)*)(dataPtr));
        protocol->info.version = ntoh(netOrderVersion);
        if(protocol->info.version != Protocol::Version_01_00_00) {
            Log::W(Log::Tag::Msg, "Unsupperted version %u", protocol->info.version);
            return ErrCode::CarrierSessionUnsuppertedVersion;
        }
        dataPtr += sizeof(protocol->info.version);

        auto netOrderHeadSize = *((typeof(protocol->info.headSize)*)(dataPtr));
        protocol->info.headSize = ntoh(netOrderHeadSize);
        dataPtr += sizeof(protocol->info.headSize);

        auto netOrderBodySize = *((typeof(protocol->info.bodySize)*)(dataPtr));
        protocol->info.bodySize = ntoh(netOrderBodySize);
        dataPtr += sizeof(protocol->info.bodySize);

        Log::D(Log::Tag::Msg, "Receiving session body start.");
    }

    // return and parse next time if data is not enough to save as head data.
    if(cachingData.size() < (sizeof(protocol->info) + protocol->info.headSize)) {
        Log::D(Log::Tag::Msg, "Protocol head data is not enough. caching size: %d", cachingData.size());
        return ErrCode::CarrierSessionDataNotEnough;
    }

    // store head data and clear cache.
    auto headDataPtr = cachingData.data() + sizeof(protocol->info);
    protocol->payload->headData = {headDataPtr, headDataPtr + protocol->info.headSize};
    cachingData.clear();

    // body offset of input data.
    auto bodyStartIdx = (sizeof(protocol->info) + protocol->info.headSize - cachingDataPrevSize);

    return bodyStartIdx;
}

int SessionParser::unpackBodyData(const std::vector<uint8_t>& data, int offset,
                                  std::shared_ptr<OnUnpackedListener> listener) {
    auto neededData = protocol->info.bodySize - protocol->payload->bodyData.receivedBodySize;
    auto realSize = (neededData < (data.size() - offset)
                  ? neededData : (data.size() - offset));

    protocol->payload->bodyData.stream.write((char*)data.data() + offset, realSize);
    protocol->payload->bodyData.receivedBodySize += realSize;

    if(protocol->payload->bodyData.receivedBodySize == protocol->info.bodySize) {
        Log::D(Log::Tag::Msg, "Receiving session body finished.");

        if(listener != nullptr) {
            protocol->payload->bodyData.stream.flush();
            protocol->payload->bodyData.stream.close();

            (*listener)(protocol->payload->headData, protocol->payload->bodyData.filepath);

        }
        protocol.reset();
    }

    return realSize;
}

int64_t SessionParser::ntoh(int64_t value) const
{
    return ntohll(value);
}

int64_t SessionParser::hton(int64_t value) const
{
    return htonll(value);
}

int32_t SessionParser::ntoh(int32_t value) const
{
    return ntohl(value);
}

int32_t SessionParser::hton(int32_t value) const
{
    return htonl(value);
}

uint32_t SessionParser::ntoh(uint32_t value) const
{
    return ntohl(value);
}

uint32_t SessionParser::hton(uint32_t value) const
{
    return htonl(value);
}

SessionParser::Protocol::Payload::Payload(const std::filesystem::path& bodyPath)
    : headData()
    , bodyData()
{
    bodyData.filepath = bodyPath;
    bodyData.stream.open(bodyData.filepath,
                         std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
    bodyData.receivedBodySize = 0;
}

SessionParser::Protocol::Payload::~Payload()
{
    bodyData.stream.close();
    std::filesystem::remove(bodyData.filepath);
}


} // namespace trinity