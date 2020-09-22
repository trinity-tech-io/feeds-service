#include "SessionParser.hpp"

#include <functional>
#include <Random.hpp>
#include <SafePtr.hpp>
#include <DateTime.hpp>


namespace elastos {

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
    // Log::W(Log::TAG, "%s datasize=%d", __PRETTY_FUNCTION__, data.size());

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
        // Log::D(Log::TAG, "%s datapos=%d", __PRETTY_FUNCTION__, dataPos);
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

    Log::V(Log::TAG, "%s datasize=%d", __PRETTY_FUNCTION__, data.size());
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
        // Log::D(Log::TAG, "Protocol has been parsed.");
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
            Log::W(Log::TAG, "Remove garbage size %d", garbageIdx);
            cachingData.erase(cachingData.begin(), cachingData.begin() + garbageIdx);
            cachingDataPrevSize -= garbageIdx; // recompute valid data of previous cache.
        }

        // return and parse next time if data is not enough to parse info.
        if(cachingData.size() < sizeof(Protocol::Info)) {
            Log::D(Log::TAG, "Protocol info data is not enough.");
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

        auto netOrderMagicNum = *((typeof(protocol->info.magicNumber)*)(dataPtr));
        protocol->info.magicNumber = ntoh(netOrderMagicNum);
        dataPtr += sizeof(protocol->info.magicNumber);

        auto netOrderVersion = *((typeof(protocol->info.version)*)(dataPtr));
        protocol->info.version = ntoh(netOrderVersion);
        if(protocol->info.version != Protocol::Version_01_00_00) {
            Log::W(Log::TAG, "Unsupperted version %u", protocol->info.version);
            return ErrCode::CarrierSessionUnsuppertedVersion;
        }
        dataPtr += sizeof(protocol->info.version);

        auto netOrderHeadSize = *((typeof(protocol->info.headSize)*)(dataPtr));
        protocol->info.headSize = ntoh(netOrderHeadSize);
        dataPtr += sizeof(protocol->info.headSize);

        auto netOrderBodySize = *((typeof(protocol->info.bodySize)*)(dataPtr));
        protocol->info.bodySize = ntoh(netOrderBodySize);
        dataPtr += sizeof(protocol->info.bodySize);

        Log::D(Log::TAG, "Transfer start. timestamp=%lld", DateTime::CurrentMS());
    }

    // return and parse next time if data is not enough to save as head data.
    if(cachingData.size() < (sizeof(protocol->info) + protocol->info.headSize)) {
        Log::D(Log::TAG, "Protocol head data is not enough.");
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
    // protocol->payload.bodyData.insert(protocol->payload.bodyData.end(),
    //                                   data.begin() + offset, data.begin() + offset + realSize);
    protocol->payload->bodyData.receivedBodySize += realSize;

    if(protocol->payload->bodyData.receivedBodySize == protocol->info.bodySize) {
        Log::D(Log::TAG, "Transfer finished. timestamp=%lld", DateTime::CurrentMS());

        if(listener != nullptr) {
            protocol->payload->bodyData.stream.flush();
            protocol->payload->bodyData.stream.close();

            (*listener)(protocol->payload->headData, protocol->payload->bodyData.filepath);

        }
        protocol.reset();
    }

    return realSize;
}

uint64_t SessionParser::ntoh(uint64_t value) const 
{
    // uint64_t rval;
    // uint8_t *data = (uint8_t*)&rval;

    // data[0] = value >> 56;
    // data[1] = value >> 48;
    // data[2] = value >> 40;
    // data[3] = value >> 32;
    // data[4] = value >> 24;
    // data[5] = value >> 16;
    // data[6] = value >> 8;
    // data[7] = value >> 0;

    // return rval;
    return ntohll(value);
}

uint64_t SessionParser::hton(uint64_t value) const 
{
    // return ntoh(value);
    return htonll(value);
}

uint32_t SessionParser::ntoh(uint32_t value) const
{
    // uint32_t rval;
    // uint8_t *data = (uint8_t*)&rval;

    // data[0] = value >> 24;
    // data[1] = value >> 16;
    // data[2] = value >> 8;
    // data[3] = value >> 0;

    // return rval;
    return ntohl(value);
}

uint32_t SessionParser::hton(uint32_t value) const 
{
    // return ntoh(value);
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
    Log::V(Log::TAG, "%s body data cache: %s", __PRETTY_FUNCTION__, bodyData.filepath.c_str());
}

SessionParser::Protocol::Payload::~Payload()
{
    bodyData.stream.close();
    // std::filesystem::remove(bodyData.filepath); // TODO
    Log::V(Log::TAG, "%s", __PRETTY_FUNCTION__);
}


} // namespace elastos