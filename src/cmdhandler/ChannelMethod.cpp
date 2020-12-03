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
    auto requestPtr = std::dynamic_pointer_cast<Rpc::GetMultiCommentsRequest>(request);
    CHECK_ASSERT(requestPtr != nullptr, ErrCode::InvalidArgument);
    const auto& params = requestPtr->params;
    responseArray.clear();

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

    auto condBy = DataBase::ConditionBy(static_cast<DataBase::ConditionField>(params.by), DataBase::ConditionIdType::Comment);
    if(condBy != nullptr) {
        if(params.lower_bound >= 0) {
            sql << " AND " << condBy << " >= " << params.lower_bound;
        }
        if(params.upper_bound >= 0) {
            sql << " AND " << condBy << " <= " << params.upper_bound;
        }
        sql << " ORDER BY " << condBy << (params.by == DataBase::ConditionField::Id ? " ASC" : " DESC");
    }
    if (params.max_count > 0) {
        sql << " LIMIT " << params.max_count;
    }
    sql << ";";

    std::shared_ptr<Rpc::GetMultiCommentsResponse> response;
    auto commentsSize = sizeof(Rpc::GetMultiCommentsResponse) - sizeof(Rpc::GetMultiCommentsResponse::Result::comments);
    DataBase::Step step = [&](SQLite::Statement& stmt) -> int {
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
                        - sizeof(comment.user_did) + comment.user_did.length()
                        - sizeof(comment.user_name) + comment.user_name.length()
                        - sizeof(comment.content) + comment.content.size();
        if(nextCommentsSize > Rpc::Factory::MaxAvailableSize) {
            responseArray.push_back(response);
            response.reset();
            commentsSize = sizeof(Rpc::GetMultiCommentsResponse) - sizeof(Rpc::GetMultiCommentsResponse::Result::comments);
        }
        if(response == nullptr) {
            auto responsePtr = Rpc::Factory::MakeResponse(request->method);
            response = std::dynamic_pointer_cast<Rpc::GetMultiCommentsResponse>(responsePtr);
            CHECK_ASSERT(response != nullptr, ErrCode::RpcUnimplementedError);
            response->version = request->version;
            response->id = request->id;
        }
        response->result.comments.push_back(std::move(comment));
        commentsSize = nextCommentsSize;

        return 0;
    };

    int ret = DataBase::GetInstance()->executeStep(sql.str(), step);
    if(ret < 0) {
        responseArray.clear();
    }
    CHECK_ERROR(ret);

    if(response != nullptr) { // push last response
        response->result.is_last = true;
        responseArray.push_back(response);
    }

    return 0;
}

} // namespace trinity