#ifndef _SESSION_PARSER_HPP_
#define _SESSION_PARSER_HPP_

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace elastos {

class SessionParser : public std::enable_shared_from_this<SessionParser> {
public:
    /*** type define ***/
 using OnUnpackedListener = std::function<void(const std::vector<uint8_t>& headData,
                                               const std::filesystem::path& bodyPath)>;

 /*** static function and variable ***/

 /*** class function and variable ***/
 explicit SessionParser() = default;
 virtual ~SessionParser() = default;

 void config(const std::filesystem::path& cacheDir);

 int unpack(const std::vector<uint8_t>& data,
            std::shared_ptr<OnUnpackedListener> listener);
 int pack(std::vector<uint8_t>& data,
          const std::vector<uint8_t>& headData,
          const std::filesystem::path& bodyPath);

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
        };
        struct Payload {
            explicit Payload(const std::filesystem::path& bodyPath);
            virtual ~Payload();
            std::vector<uint8_t> headData;
            struct {
                std::filesystem::path filepath;
                std::fstream stream;
                uint64_t receivedBodySize;
            } bodyData;
        };

        Info info;
        std::unique_ptr<Payload> payload;

    private:
        static constexpr const uint64_t MagicNumber = 0xA5A5202008275A5A;
        static constexpr const uint64_t Version_01_00_00 = 10000;

        friend SessionParser;
    };

    /*** static function and variable ***/
    static constexpr const char* BodyCacheName = "session-bodydata-";

    /*** class function and variable ***/
    int unpackProtocol(const std::vector<uint8_t>& data, int offset);
    int unpackBodyData(const std::vector<uint8_t>& data, int offset,
                       std::shared_ptr<OnUnpackedListener> listener);
    uint64_t ntoh(uint64_t value) const;
    uint64_t hton(uint64_t value) const;
    uint32_t ntoh(uint32_t value) const;
    uint32_t hton(uint32_t value) const;
    // reserved
    // uint16_t ntoh(uint16_t value) const;
    // uint16_t hton(uint16_t value) const;

    std::filesystem::path bodyCacheDir;

    std::unique_ptr<Protocol> protocol;
    std::vector<uint8_t> cachingData;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace elastos

#endif /* _SESSION_PARSER_HPP_ */


