#include "MassDataProcessor.hpp"

#include <cstring>
#include <ela_carrier.h>
#include <ela_session.h>
#include <functional>
#include <SafePtr.hpp>

#include <crystal.h>
extern "C" {
#define new fix_cpp_keyword_new
#include <auth.h>
#include <did.h>
#undef new
}

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
MassDataProcessor::MassDataProcessor(const std::filesystem::path& massDataDir)
    : MassData(massDataDir)
{
    using namespace std::placeholders;
    mothodHandleMap = {
        {MassData::Method::SetBinary, {std::bind(&MassDataProcessor::onSetBinary, this, _1, _2, _3), MassData::Accessible::Owner}},
        {MassData::Method::GetBinary, {std::bind(&MassDataProcessor::onGetBinary, this, _1, _2, _3), MassData::Accessible::Member}},
    };
}

MassDataProcessor::~MassDataProcessor()
{
}

int MassDataProcessor::dispose(const std::vector<uint8_t>& headData,
                               const std::filesystem::path& bodyPath)
{
    std::shared_ptr<Req> req;
    std::shared_ptr<Resp> resp;
    int ret = CommandHandler::GetInstance()->unpackRequest(headData, req);
    if(ret >= 0) {
        Log::D(Log::Tag::Msg, "Mass data processor: dispose method [%s]", req->method);
        ret = ErrCode::UnimplementedError;
        for (const auto& it : mothodHandleMap) {
            if (std::strcmp(it.first, req->method) != 0) {
                continue;
            }

            ret = checkAccessible(it.second.accessible, reinterpret_cast<TkReq*>(req.get())->params.tk);
            if(ret < 0) {
                break;
            }

            ret = it.second.callback(req, bodyPath, resp);
            break;
        }
    }

    auto errCode = ret;
    std::vector<uint8_t> data;
    ret = CommandHandler::GetInstance()->packResponse(req, resp, errCode, data);
    CHECK_ERROR(ret);

    resultHeadData = std::move(data);

    return 0;
}

int MassDataProcessor::getResultAndReset(std::vector<uint8_t>& headData,
                                         std::filesystem::path& bodyPath)
{
    headData = std::move(resultHeadData);
    bodyPath = std::move(resultBodyPath);

    resultHeadData.clear();
    resultBodyPath.clear();

    return 0;
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */

int MassDataProcessor::onSetBinary(const std::shared_ptr<Req>& req,
                                   const std::filesystem::path& bodyPath,
                                   std::shared_ptr<Resp>& resp)
{
    int ret;

    ret = MassData::saveBinData(massDataDir, req, resp, bodyPath);
    CHECK_ERROR(ret);

    return 0;
}

int MassDataProcessor::onGetBinary(const std::shared_ptr<Req>& req,
                                   const std::filesystem::path& bodyPath,
                                   std::shared_ptr<Resp>& resp)
{
    std::ignore = bodyPath;
    int ret;

    ret = MassData::loadBinData(massDataDir, req, resp, resultBodyPath);
    CHECK_ERROR(ret);

    return 0;
}

} // namespace trinity