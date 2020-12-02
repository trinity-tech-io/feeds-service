#include "ChannelMethod.hpp"

#include <sstream>
#include <DataBase.hpp>
#include <ErrCode.hpp>
#include <Log.hpp>
#include <RpcFactory.hpp>
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
    responseArray.clear();
    auto indeedRequest = std::dynamic_pointer_cast<Rpc::GetMultiCommentsRequest>(request);
    CHECK_ASSERT(indeedRequest != nullptr, ErrCode::InvalidArgument);
    const auto& params = indeedRequest->params;
    Log::D(Log::TAG, "Request params:");
    Log::D(Log::TAG, "    access_token: %s", params.access_token.c_str());
    Log::D(Log::TAG, "    channel_id  : %lld", params.channel_id);
    Log::D(Log::TAG, "    post_id     : %lld", params.post_id);
    Log::D(Log::TAG, "    by          : %lld", params.by);
    Log::D(Log::TAG, "    upper_bound : %lld", params.upper_bound);
    Log::D(Log::TAG, "    lower_bound : %lld", params.lower_bound);
    Log::D(Log::TAG, "    max_count   : %lld", params.max_count);
    Log::D(Log::TAG, " ");

    bool validArgus = ( params.access_token.empty() == false
                     && params.channel_id >= 0
                     && params.post_id >= 0);
    CHECK_ASSERT(validArgus, ErrCode::InvalidArgument);

    std::vector<std::function<void()>> sqlBindArray;
    std::stringstream sql;
    sql << " SELECT channel_id, post_id, comment_id, refcomment_id,";
    sql << " did, name,";
    sql << " status, likes, created_at, updated_at, content";
    sql << " FROM comments JOIN users USING (user_id)";
    params.channel_id > 0 ? (sql << " WHERE channel_id = " << params.channel_id)
                          : (sql << " WHERE channel_id = " << "channel_id");
    params.post_id > 0 ? (sql << " AND post_id = " << params.post_id)
                       : (sql << " AND post_id = " << "post_id");

    auto queryBy = DataBase::QueryBy(static_cast<DataBase::QueryField>(params.by), DataBase::QueryIdType::Comment);
    if(queryBy != nullptr) {
        if(params.lower_bound >= 0) {
            sql << " AND " << queryBy << " >= " << params.lower_bound;
        }
        if(params.upper_bound >= 0) {
            sql << " AND " << queryBy << " <= " << params.upper_bound;
        }
        sql << " ORDER BY " << queryBy << (params.by == DataBase::QueryField::Id ? " ASC" : " DESC");
    }
    if (params.max_count > 0) {
        sql << " LIMIT " << params.max_count;
    }
    sql << ";";

    try {
        Log::D(Log::TAG, "Get multi comments sql: %s", sql.str().c_str());
        SQLite::Statement stmt(*DataBase::GetInstance()->getHandler(), sql.str());

        std::shared_ptr<Rpc::GetMultiCommentsResponse> response;
        int commentsSize = sizeof(Rpc::GetMultiCommentsResponse);
        while (stmt.executeStep()) {
            Rpc::GetMultiCommentsResponse::Result::Comment comment;
            comment.channel_id = stmt.getColumn(0).getInt64();
            comment.post_id = stmt.getColumn(1).getInt64();
            comment.comment_id = stmt.getColumn(2).getInt64();
            comment.refer_comment_id = stmt.getColumn(3).getInt64();
            comment.user_did = stmt.getColumn(4).getString();
            comment.user_name = stmt.getColumn(5).getString();
            comment.status = stmt.getColumn(6).getInt64();
            comment.likes = stmt.getColumn(7).getInt64();
            comment.created_at = stmt.getColumn(8).getInt64();
            comment.updated_at = stmt.getColumn(9).getInt64();
            comment.content = std::move(std::vector<uint8_t> {
                (uint8_t*)stmt.getColumn(10).getBlob(),
                (uint8_t*)stmt.getColumn(10).getBlob() + stmt.getColumn(10).getBytes()
            });

            auto nextCommentsSize = commentsSize + sizeof(comment)
                         + comment.user_did.length() + comment.user_name.length()
                         + comment.content.size();
            if(nextCommentsSize > Rpc::Factory::MaxAvailableSize) {
                responseArray.push_back(response);
                response.reset();
                commentsSize = sizeof(Rpc::GetMultiCommentsResponse);
            }
            if(response == nullptr) {
                auto ptr = Rpc::Factory::MakeResponse(request->method);
                response = std::dynamic_pointer_cast<Rpc::GetMultiCommentsResponse>(ptr);
                CHECK_ASSERT(response != nullptr, ErrCode::RpcUnimplementedError);
                response->id = request->id;
            }
            response->result.comments.push_back(std::move(comment));
            commentsSize = nextCommentsSize;
        }
        if(response != nullptr) { // push last response
            response->result.is_last = true;
            responseArray.push_back(response);
        }
    } catch (SQLite::Exception& e) {
        Log::E(Log::TAG, "Failed to get multi comments. exception: %s", e.what());
        CHECK_ERROR(ErrCode::DBException);
    }

    for (const auto& response : responseArray) {
        const auto& result = std::dynamic_pointer_cast<Rpc::GetMultiCommentsResponse>(response)->result;
        Log::D(Log::TAG, "Response result:");
        Log::D(Log::TAG, "    is_last: %d", result.is_last);
        Log::D(Log::TAG, "    comments:");
        for(const auto& it: result.comments) {
            Log::D(Log::TAG, "        channel_id: %lld", it.channel_id);
            Log::D(Log::TAG, "        post_id: %lld", it.post_id);
            Log::D(Log::TAG, "        comment_id: %lld", it.comment_id);
            Log::D(Log::TAG, "        refer_comment_id: %lld", it.refer_comment_id);
            Log::D(Log::TAG, "        status: %lld", it.status);
            Log::D(Log::TAG, "        user_did: %s", it.user_did.c_str());
            Log::D(Log::TAG, "        user_name: %s", it.user_name.c_str());
            Log::D(Log::TAG, "        content_size: %d", it.content.size());
            Log::D(Log::TAG, "        likes: %lld", it.likes);
            Log::D(Log::TAG, "        created_at: %lld", it.created_at);
            Log::D(Log::TAG, "        updated_at: %lld", it.updated_at);
            Log::D(Log::TAG, " ");
        }
    }

    return 0;
}

} // namespace trinity