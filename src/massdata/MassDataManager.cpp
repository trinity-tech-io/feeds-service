#include "MassDataManager.hpp"

#include <ela_carrier.h>
#include <ela_session.h>
#include <functional>
#include <SafePtr.hpp>
#include "CarrierSession.hpp"
#include "MassDataProcessor.hpp"
#include "SessionParser.hpp"

namespace elastos {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */
std::shared_ptr<MassDataManager> MassDataManager::MassDataMgrInstance;

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */
std::shared_ptr<MassDataManager> MassDataManager::GetInstance()
{
    // ignore thread-safty, no needed

    if(MassDataMgrInstance != nullptr) {
        return MassDataMgrInstance;
    }

    struct Impl: MassDataManager {
    };
    MassDataMgrInstance = std::make_shared<Impl>();

    return MassDataMgrInstance;
}

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */
int MassDataManager::config(const std::filesystem::path& dataDir,
                            std::weak_ptr<ElaCarrier> carrier)
{
    massDataDir = dataDir / MassDataDirName;

    Log::I(Log::TAG, "Mass data saved to: %s", massDataDir.c_str());
    auto dirExists = std::filesystem::exists(massDataDir / MassDataCacheDirName);
    if(dirExists == false) {
        auto dirExists = std::filesystem::create_directories(massDataDir / MassDataCacheDirName);
        CHECK_ASSERT(dirExists, ErrCode::FileNotExistsError);
    }

    using namespace std::placeholders;
    int ret = CarrierSession::Factory::Init(carrier,
                                            std::bind(&MassDataManager::onSessionRequest, this, _1, _2, _3));
    CHECK_ERROR(ret);

    return 0;
}

void MassDataManager::cleanup()
{
    CarrierSession::Factory::Uninit();

    MassDataMgrInstance.reset();
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
void MassDataManager::onSessionRequest(std::weak_ptr<ElaCarrier> carrier,
                                       const std::string& from, const std::string& sdp)
{
    Log::D(Log::TAG, "%s", __PRETTY_FUNCTION__);

    auto dataPipe = std::make_shared<DataPipe>();
    dataPipe->session = CarrierSession::Factory::Create();
    dataPipe->parser = std::make_shared<SessionParser>();
    dataPipe->processor = std::make_shared<MassDataProcessor>();

    // config session.
    auto unpackedListener = makeUnpackedListener(from);
    auto connectListener = makeConnectListener(from, unpackedListener);

    dataPipe->session->setSdp(sdp);
    int ret = dataPipe->session->allowConnect(from, connectListener);
    CHECK_RETVAL(ret);

    // config parser.
    dataPipe->parser->config(massDataDir / MassDataCacheDirName);
    dataPipe->processor->config(massDataDir);

    append(from, dataPipe);
}

void MassDataManager::append(const std::string& key, std::shared_ptr<MassDataManager::DataPipe> value)
{
    dataPipeMap.emplace(key, value);
}

void MassDataManager::remove(const std::string& key)
{
    dataPipeMap.erase(key);
}

std::shared_ptr<MassDataManager::DataPipe> MassDataManager::find(const std::string& key)
{
    auto dataPipeIt = dataPipeMap.find(key);
    if(dataPipeIt == dataPipeMap.end()) {
        CHECK_AND_RETDEF(ErrCode::CarrierSessionReleasedError, nullptr);
    }
    auto dataPipe = dataPipeIt->second;

    return dataPipe; 
}

std::shared_ptr<CarrierSession::ConnectListener> MassDataManager::makeConnectListener(const std::string& peerId,
                                                                                      std::shared_ptr<SessionParser::OnUnpackedListener> unpackedListener) {
    struct SessionListener: CarrierSession::ConnectListener {
        explicit SessionListener(std::shared_ptr<MassDataManager> mgr,
                                 const std::string& peerId,
                                 std::shared_ptr<SessionParser::OnUnpackedListener> unpackedListener) {
            this->mgr = mgr;
            this->peerId = peerId;
            this->unpackedListener = unpackedListener;
        }

        virtual void onNotify(Notify notify, int errCode) override {
            Log::D(Log::TAG, "%s", __PRETTY_FUNCTION__);

            if(notify == Notify::Closed
            || notify == Notify::Error) {
                auto mgrPtr = SAFE_GET_PTR_NO_RETVAL(mgr);
                mgrPtr->remove(peerId);
            }
        };
        virtual void onReceivedData(const std::vector<uint8_t>& data) override {
            Log::D(Log::TAG, "%s", __PRETTY_FUNCTION__);

            auto mgrPtr = SAFE_GET_PTR_NO_RETVAL(mgr);
            auto dataPipe = mgrPtr->find(peerId);
            assert(dataPipe->parser != nullptr);

            int ret = dataPipe->parser->unpack(data, unpackedListener);
            CHECK_RETVAL(ret);
        }

    private:
        std::weak_ptr<MassDataManager> mgr;
        std::string peerId;
        std::shared_ptr<SessionParser::OnUnpackedListener> unpackedListener;
    };
    auto sessionListener = std::make_shared<SessionListener>(shared_from_this(), peerId, unpackedListener);

    return sessionListener;
}

std::shared_ptr<SessionParser::OnUnpackedListener> MassDataManager::makeUnpackedListener(const std::string& peerId)
{
    auto unpackedListener = std::make_shared<SessionParser::OnUnpackedListener>(
        [=](const std::vector<uint8_t>& headData,
            const std::filesystem::path& bodyPath) -> void {
            auto mgrPtr = shared_from_this();
            auto dataPipe = mgrPtr->find(peerId);
            assert(dataPipe->processor != nullptr);

            int ret = dataPipe->processor->dispose(headData, bodyPath);
            CHECK_RETVAL(ret);

            std::vector<uint8_t> resultHeadData;
            std::filesystem::path resultBodyPath;
            ret = dataPipe->processor->getResult(resultHeadData, resultBodyPath);
            CHECK_RETVAL(ret);

            std::vector<uint8_t> sessionProtocolData;
            ret = dataPipe->parser->pack(sessionProtocolData, resultHeadData, resultBodyPath);                                                                            
            CHECK_RETVAL(ret);

            ret = dataPipe->session->sendData(sessionProtocolData);
            CHECK_RETVAL(ret);
            ret = dataPipe->session->sendData(resultHeadData);
            CHECK_RETVAL(ret);

            if(std::filesystem::exists(resultBodyPath) == true) {
                std::fstream bodyStream;
                bodyStream.open(resultBodyPath, std::ios::binary);
                ret = dataPipe->session->sendData(bodyStream);
                bodyStream.close();
                CHECK_RETVAL(ret);
            }
        }
    );

    return unpackedListener;
}

} // namespace elastos