#include "AutoUpdate.hpp"

namespace trinity {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */
std::shared_ptr<AutoUpdate> AutoUpdate::AutoUpdateInstance;

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */
std::shared_ptr<AutoUpdate> AutoUpdate::GetInstance()
{
    // ignore check thread-safty, no needed

    if(AutoUpdateInstance != nullptr) {
        return AutoUpdateInstance;
    }

    struct Impl: AutoUpdate {
    };
    AutoUpdateInstance = std::make_shared<Impl>();

    return AutoUpdateInstance;
}

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */

} // namespace trinity
