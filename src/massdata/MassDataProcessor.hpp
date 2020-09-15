#ifndef _MASSDATA_PROCESSOR_HPP_
#define _MASSDATA_PROCESSOR_HPP_

#include <memory>
#include <string>
#include <vector>
#include <fstream>

namespace elastos {

class MassDataProcessor : public std::enable_shared_from_this<MassDataProcessor> {
public:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit MassDataProcessor() = default;
    virtual ~MassDataProcessor() = default;

    int dispose(const std::vector<uint8_t>& data);

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/
    struct Protocol {
        struct Info {
            uint64_t magicNumber;
            uint32_t version;
            uint32_t headSize;
            uint64_t bodySize;
        } info;
        struct Payload {
            explicit Payload();
            virtual ~Payload();
            std::vector<uint8_t> headData;
            // std::fstream bodyData;
            std::vector<uint8_t> bodyData;
            uint64_t receivedBodySize;
        } payload;
    };

    /*** static function and variable ***/
    static constexpr const char* MassDataCacheName = "massdata-";
    static constexpr const uint64_t MagicNumber = 0xA5A5202008275A5A;
    static constexpr const uint64_t Version_01_00_00 = 10000;

    /*** class function and variable ***/
    int parseProtocol(const std::vector<uint8_t>& data, int offset);
    int storeData(const std::vector<uint8_t>& data, int offset);
    uint64_t ntoh(uint64_t value) const;
    uint64_t hton(uint64_t value) const;
    uint32_t ntoh(uint32_t value) const;
    uint32_t hton(uint32_t value) const;
    // reserved
    // uint16_t ntoh(uint16_t value) const;
    // uint16_t hton(uint16_t value) const;

    std::unique_ptr<Protocol> protocol;
    std::vector<uint8_t> dataCache;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace elastos

#endif /* _MASSDATA_PROCESSOR_HPP_ */


