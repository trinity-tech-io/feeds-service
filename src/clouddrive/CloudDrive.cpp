#include "CloudDrive.hpp"

#include <ErrCode.hpp>
#include <Log.hpp>
#include <OneDrive.hpp>

namespace trinity {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */
std::shared_ptr<CloudDrive> CloudDrive::Create(Type type,
                                               const std::string& driveUrl,
                                               const std::string& accessToken)
{
    std::shared_ptr<CloudDrive> drive;

    switch (type) {
    case Type::OneDrive:
        {
            struct Impl: trinity::OneDrive {
                explicit Impl(const std::string& driveUrl, const std::string& accessToken)
                    : OneDrive(driveUrl, accessToken) {}
                virtual ~Impl() {};
            };
            drive = std::make_shared<Impl>(driveUrl, accessToken);
        }
        break;
    default:
        Log::E(Log::Tag::Cmd, "Failed to create cloud drive by type %d.", type);
        break;
    }

    return drive;
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


