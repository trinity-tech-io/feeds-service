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
    auto deleter = [](void* ptr) -> void {
        deref(ptr);
    };

    Req *reqBuf = nullptr;
    int ret = rpc_unmarshal_req(headData.data(), headData.size(), &reqBuf);
    auto req = std::shared_ptr<Req>(reqBuf, deleter); // workaround: declare for auto release Req pointer
    auto resp = std::make_shared<Resp>();
    if (ret >= 0) {
        Log::D(Log::TAG, "Mass data processor: dispose method [%s]", req->method);
        ret = ErrCode::UnimplementedError;
        for (const auto& it : mothodHandleMap) {
            if (std::strcmp(it.first, req->method) != 0) {
                continue;
            }

            ret = checkAccessible(it.second.accessible, reinterpret_cast<TkReq*>(req.get())->params.tk);
            CHECK_ERROR(ret);

            ret = it.second.callback(req, bodyPath, resp);
            break;
        }
    } else if (ret == -1) {
        ret = ErrCode::MassDataUnmarshalReqFailed;
    } else if (ret == -2) {
        ret = ErrCode::MassDataUnknownReqFailed;
    } else if (ret == -3) {
        ret = ErrCode::MassDataUnsupportedVersion;
    } else {
        ret = ErrCode::UnknownError;
    }
    if(ret < ErrCode::StdSystemErrorIndex) {
        ret = ErrCode::StdSystemError;
    }

    Marshalled* marshalBuf = nullptr;
    if(ret >= 0) {
        marshalBuf = rpc_marshal_resp(req->method, resp.get());
    } else {
        CHECK_ASSERT(req, ret);
        auto errDesp = ErrCode::ToString(ret);
        marshalBuf = rpc_marshal_err(req->tsx_id, ret, errDesp.c_str());
        Log::D(Log::TAG, "Response error:");
        Log::D(Log::TAG, "    code: %d", ret);
        Log::D(Log::TAG, "    message: %s", errDesp.c_str());
    }
    auto marshalData = std::shared_ptr<Marshalled>(marshalBuf, deleter); // workaround: declare for auto release Marshalled pointer
    CHECK_ASSERT(marshalData, ErrCode::MassDataMarshalRespFailed);

    auto marshalDataPtr = reinterpret_cast<uint8_t*>(marshalData->data);
    resultHeadData = {marshalDataPtr, marshalDataPtr + marshalData->sz};

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

int MassDataProcessor::onSetBinary(std::shared_ptr<Req> req,
                                   const std::filesystem::path& bodyPath,
                                   std::shared_ptr<Resp>& resp)
{
    int ret;

    ret = MassData::saveBinData(massDataDir, req, resp, bodyPath);
    CHECK_ERROR(ret);

    return 0;
}

int MassDataProcessor::onGetBinary(std::shared_ptr<Req> req,
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