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
    std::map<const char*, AdvancedHandler> advancedHandlerMap {
        {Rpc::Factory::Method::GetMultiComments,  {std::bind(&ChannelMethod::onGetMultiComments, this, _1, _2), Accessible::Member}},
    };

    setHandleMap({}, advancedHandlerMap);
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
int ChannelMethod::onGetMultiComments(std::shared_ptr<Rpc::Request> request,
                                      std::vector<std::shared_ptr<Rpc::Response>>& responseArray)
{
    auto indeedRequest = std::dynamic_pointer_cast<Rpc::GetMultiCommentsRequest>(request);
    CHECK_ASSERT(indeedRequest != nullptr, ErrCode::InvalidArgument);
    Log::D(Log::TAG, "Request params:");
    Log::D(Log::TAG, "    access_token: %s", indeedRequest->params.access_token.c_str());
    Log::D(Log::TAG, "    channel_id  : %lld", indeedRequest->params.channel_id);
    Log::D(Log::TAG, "    post_id  : %lld", indeedRequest->params.post_id);
    Log::D(Log::TAG, "    by  : %lld", indeedRequest->params.by);
    Log::D(Log::TAG, "    upper_bound  : %lld", indeedRequest->params.upper_bound);
    Log::D(Log::TAG, "    lower_bound  : %lld", indeedRequest->params.lower_bound);
    Log::D(Log::TAG, "    max_count  : %lld", indeedRequest->params.max_count);

    bool validArgus = ( indeedRequest->id > 0
                     && indeedRequest->params.access_token.empty() == false
                     && indeedRequest->params.channel_id >= 0
                     && indeedRequest->params.post_id >= 0 );
    CHECK_ASSERT(validArgus, ErrCode::InvalidArgument);


    return -1;
}

} // namespace trinity