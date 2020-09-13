#include "MassDataManager.hpp"

#include <ela_carrier.h>
#include <ela_session.h>
#include <functional>
#include <SafePtr.hpp>
#include "CarrierSession.hpp"

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
int MassDataManager::config(std::weak_ptr<ElaCarrier> carrier)
{
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

    auto session = CarrierSession::Factory::Create();
    if(session == nullptr) {
        CHECK_RETVAL(ErrCode::OutOfMemoryError);
    }

    session->setSdp(sdp);

    struct Impl: CarrierSession::ConnectListener {
        explicit Impl(std::weak_ptr<MassDataManager> mgr) {
            this->mgr = mgr;
        }

        virtual void onNotify(Notify notify, int errCode) override {
            Log::D(Log::TAG, "%s", __PRETTY_FUNCTION__);

            if(notify == Notify::Closed) {
                auto ptr = SAFE_GET_PTR_NO_RETVAL(mgr);
                ptr->carrierSessionMap.erase(this);
            }
        };
        virtual void onReceivedData(const std::vector<uint8_t>& data) override {
            Log::D(Log::TAG, "%s", __PRETTY_FUNCTION__);
        }

    private:
        std::weak_ptr<MassDataManager> mgr;
    };
    auto impl = std::make_shared<Impl>(weak_from_this());

    int ret = session->allowConnect(from, impl);
    CHECK_RETVAL(ret);

    carrierSessionMap.emplace(impl.get(), session);
}

} // namespace elastos