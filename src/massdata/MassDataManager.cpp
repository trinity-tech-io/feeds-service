#include "MassDataManager.hpp"

#include <cassert>
#include <ela_carrier.h>
#include <ela_session.h>
#include <functional>
#include <SafePtr.hpp>
#include "CarrierSession.hpp"
#include "MassDataProcessor.hpp"
#include "SessionParser.hpp"
#include "DateTime.hpp"

namespace trinity {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */
std::shared_ptr<MassDataManager> MassDataManager::MassDataMgrInstance;

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */
std::shared_ptr<MassDataManager> MassDataManager::GetInstance()
{
    // ignore check thread-safty, no needed

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
    massDataDir = dataDir / MassData::MassDataDirName;

    Log::D(Log::TAG, "Mass data saved to: %s", massDataDir.c_str());
    auto dirExists = std::filesystem::exists(massDataDir);
    if(dirExists == false) {
        auto dirExists = std::filesystem::create_directories(massDataDir);
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
    Log::D(Log::TAG, "Received carrier session request from %s. sdp:\n%s",
                     from.c_str(), sdp.c_str());

    auto dataPipe = std::make_shared<DataPipe>();
    dataPipe->session = CarrierSession::Factory::Create();
    dataPipe->parser = std::make_shared<SessionParser>();
    dataPipe->processor = std::make_shared<MassDataProcessor>(massDataDir);

    // config session.
    auto unpackedListener = makeUnpackedListener(from);
    auto connectListener = makeConnectListener(from, unpackedListener);

    dataPipe->session->setSdp(sdp);
    int ret = dataPipe->session->allowConnectAsync(from, connectListener);
    CHECK_RETVAL(ret);

    // config parser.
    dataPipe->parser->config(massDataDir / MassData::MassDataCacheDirName);

    appendDataPipe(from, dataPipe);
}

void MassDataManager::appendDataPipe(const std::string& key, std::shared_ptr<MassDataManager::DataPipe> value)
{
    Log::D(Log::TAG, "append datapipe key=%s,val=%p", key.c_str(), value->session.get());
    dataPipeMap[key] = value;
}

void MassDataManager::removeDataPipe(const std::string& key)
{
    Log::D(Log::TAG, "remove datapipe key=%s", key.c_str());
    dataPipeMap.erase(key);
}

void MassDataManager::clearAllDataPipe()
{
    Log::D(Log::TAG, "clear all datapipe.");
    dataPipeMap.clear();
}

std::shared_ptr<MassDataManager::DataPipe> MassDataManager::find(const std::string& key)
{
    auto dataPipeIt = dataPipeMap.find(key);
    if(dataPipeIt == dataPipeMap.end()) {
        CHECK_AND_RETDEF(ErrCode::CarrierSessionReleasedError, nullptr);
    }
    auto value = dataPipeIt->second;

    return value; 
}

std::shared_ptr<CarrierSession::ConnectListener> MassDataManager::makeConnectListener(const std::string& peerId,
                                                                                      std::shared_ptr<SessionParser::OnUnpackedListener> unpackedListener) {
    struct SessionListener: CarrierSession::ConnectListener {
        explicit SessionListener(std::weak_ptr<MassDataManager> mgr,
                                 const std::string& peerId,
                                 std::shared_ptr<SessionParser::OnUnpackedListener> unpackedListener) {
            this->mgr = mgr;
            this->peerId = peerId;
            this->unpackedListener = unpackedListener;
        }

        virtual void onNotify(Notify notify, int errCode) override {
            Log::D(Log::TAG, "Session nofify: notify:%s, errCode:%d", toString(notify), errCode);

            if(notify == Notify::Closed
            || notify == Notify::Error) {
                auto mgrPtr = SAFE_GET_PTR_NO_RETVAL(mgr);
                mgrPtr->removeDataPipe(peerId);
            }
        };
        virtual void onReceivedData(const std::vector<uint8_t>& data) override {
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
    auto sessionListener = std::make_shared<SessionListener>(weak_from_this(), peerId, unpackedListener);

    return sessionListener;
}

std::shared_ptr<SessionParser::OnUnpackedListener> MassDataManager::makeUnpackedListener(const std::string& peerId)
{
    auto unpackedListener = std::make_shared<SessionParser::OnUnpackedListener>([=](
            const std::vector<uint8_t>& headData,
            const std::filesystem::path& bodyPath) -> void
    {
        Log::D(Log::TAG, "MassData: start to process unpacked data.");
        auto weakPtr = this->weak_from_this();
        auto mgrPtr = SAFE_GET_PTR_NO_RETVAL(weakPtr);

        auto dataPipe = mgrPtr->find(peerId);
        assert(dataPipe->processor != nullptr);

        int ret = dataPipe->processor->dispose(headData, bodyPath);
        CHECK_RETVAL(ret);

        std::vector<uint8_t> resultHeadData;
        std::filesystem::path resultBodyPath;
        ret = dataPipe->processor->getResultAndReset(resultHeadData, resultBodyPath);
        CHECK_RETVAL(ret);

        std::vector<uint8_t> sessionProtocolData;
        ret = dataPipe->parser->pack(sessionProtocolData, resultHeadData, resultBodyPath);                                                                            
        CHECK_RETVAL(ret);

        ret = dataPipe->session->sendData(sessionProtocolData);
        CHECK_RETVAL(ret);
        ret = dataPipe->session->sendData(resultHeadData);
        CHECK_RETVAL(ret);

        if(resultBodyPath.empty() == false
        && std::filesystem::exists(resultBodyPath) == true) {
            auto future = std::async(std::launch::async,
                                     [=] {
                std::fstream bodyStream;
                bodyStream.open(resultBodyPath, std::ios::binary | std::ios::in | std::ios::out);
                int ret = dataPipe->session->sendData(bodyStream);
                bodyStream.close();
                CHECK_RETVAL(ret);
            });
            future.get();
        }

        Log::D(Log::TAG, "MassData: finish to process unpacked data.");
    });

    return unpackedListener;
}

} // namespace trinity