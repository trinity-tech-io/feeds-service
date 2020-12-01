#include "ChannelMethod.hpp"

#include <ErrCode.hpp>
#include <Log.hpp>
#include <SafePtr.hpp>

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
ChannelMethod::ChannelMethod()
{
    using namespace std::placeholders;
    std::map<const char*, MultiRespHandler> multiRespHandlerMap {
        {Method::GetMultiComments,  {std::bind(&ChannelMethod::onGetMultiComments, this, _1, _2), Accessible::Anyone}},
    };

    setHandleMap({}, multiRespHandlerMap);
}

ChannelMethod::~ChannelMethod()
{
}

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */


/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */
int ChannelMethod::onGetMultiComments(std::shared_ptr<Req> req,
                                      std::vector<std::shared_ptr<Resp>>& respArray)
{

    return ErrCode::UnimplementedError;
}

} // namespace trinity