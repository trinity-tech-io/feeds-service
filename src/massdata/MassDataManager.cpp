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
    dataPipe->session->setSdp(sdp);

    struct Impl: CarrierSession::ConnectListener {
        explicit Impl(std::shared_ptr<MassDataManager> mgr, const std::string& peerId) {
            this->mgr = mgr;
            this->peerId = peerId;
        }

        virtual void onNotify(Notify notify, int errCode) override {
            Log::D(Log::TAG, "%s", __PRETTY_FUNCTION__);

            if(notify == Notify::Closed
            || notify == Notify::Error) {
                auto mgrPtr = SAFE_GET_PTR_NO_RETVAL(mgr);
                mgrPtr->dataPipeMap.erase(peerId);
            }
        };
        virtual void onReceivedData(const std::vector<uint8_t>& data) override {
            Log::D(Log::TAG, "%s", __PRETTY_FUNCTION__);

            auto mgrPtr = SAFE_GET_PTR_NO_RETVAL(mgr);
            auto dataPipeIt = mgrPtr->dataPipeMap.find(peerId);
            if(dataPipeIt == mgrPtr->dataPipeMap.end()) {
                CHECK_RETVAL(ErrCode::CarrierSessionReleasedError);
            }
            auto dataPipe = dataPipeIt->second;
            assert(dataPipe->parser != nullptr);

            int ret = dataPipe->parser->dispose(data);
            CHECK_RETVAL(ret);
        }

    private:
        std::weak_ptr<MassDataManager> mgr;
        std::string peerId;
    };
    auto impl = std::make_shared<Impl>(shared_from_this(), from);
    int ret = dataPipe->session->allowConnect(from, impl);
    CHECK_RETVAL(ret);

    // config parser.
    dataPipe->parser->config(massDataDir, nullptr);

    dataPipeMap.emplace(from, dataPipe);
}

} // namespace elastos