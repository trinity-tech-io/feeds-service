#ifndef _FEEDS_RPC_DECLARE_HPP_
#define _FEEDS_RPC_DECLARE_HPP_

#include <string>
#include <MsgPackExtension.hpp>

namespace trinity {
namespace Rpc {

#define DECLARE_DEFAULT(name)  \
    explicit name() = default; \
    virtual ~name() = default

#define DECLARE_WITHTOKEN(name)        \
    explicit name()                    \
         : params()                    \
         , RequestWithToken(params){}; \
    virtual ~name() = default

struct Request {
    std::string version;
    std::string method;
    int64_t id = -1;
    MSGPACK_DEFINE_BASE(version, method, id);
    DECLARE_DEFAULT(Request);
};

struct Response {
    int64_t id = -1;
    MSGPACK_DEFINE_BASE(id);
    DECLARE_DEFAULT(Response);
};

struct RequestWithToken : Request {
    struct Params {
        std::string access_token;
        MSGPACK_DEFINE_BASE(access_token);
    };

    const Params& params;
    explicit RequestWithToken(Params& params) : params(params){};
    virtual ~RequestWithToken() = default;
};

struct GetMultiCommentsRequest : RequestWithToken {
    struct Params : RequestWithToken::Params {
        int64_t channel_id = -1;
        int64_t post_id = -1;
        int64_t by = -1;
        int64_t upper_bound = -1;
        int64_t lower_bound = -1;
        int64_t max_count = -1;
        MSGPACK_DEFINE_INHERIT(RequestWithToken::Params,
                               channel_id, post_id, by, upper_bound, lower_bound, max_count);
    };

    Params params;
    MSGPACK_DEFINE_INHERIT(RequestWithToken, params);
    DECLARE_WITHTOKEN(GetMultiCommentsRequest);
};

struct GetMultiCommentsResponse : Response {
    struct Result {
        struct Comment {
            int64_t channel_id = -1;
            int64_t post_id = -1;
            int64_t comment_id = -1;
            int64_t refer_comment_id = -1;
            int64_t status = -1;
            std::string user_did;
            std::string user_name;
            std::vector<uint8_t> content;
            int64_t likes = -1;
            int64_t created_at = -1;
            int64_t updated_at = -1;
            MSGPACK_DEFINE_BASE(channel_id, post_id, comment_id, refer_comment_id,
                                status, user_did, user_name, content, likes, created_at, updated_at);
        };

        bool is_last;
        std::vector<Comment> comments;
        MSGPACK_DEFINE_BASE(is_last, comments);
    };

    Result result;
    MSGPACK_DEFINE_INHERIT(Response, result);
    DECLARE_DEFAULT(GetMultiCommentsResponse);
};

} // namespace Rpc
} // namespace trinity

#endif /* _FEEDS_RPC_DECLARE_HPP_ */


