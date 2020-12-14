#ifndef _FEEDS_RPC_DECLARE_HPP_
#define _FEEDS_RPC_DECLARE_HPP_

#include <MsgPackExtension.hpp>

namespace trinity {
namespace Rpc {

struct Request {
    std::string version;
    std::string method;
    int64_t id = -1;
    MSGPACK_DEFINE_STRUCT(Request, MSGPACK_REQUEST_ARGS);
};

struct Response {
    std::string version;
    int64_t id = -1;
    MSGPACK_DEFINE_STRUCT(Response, MSGPACK_RESPONSE_ARGS);
};

struct RequestWithToken : Request {
    struct Params {
        std::string access_token;
    };

    DECLARE_DEFAULT_STRUCT(RequestWithToken);
    virtual const std::string& accessToken() = 0;
};

struct StandardSignInRequest : Request {
    struct Params {
        std::string document;
        MSGPACK_DEFINE(document);
    };

    Params params;
    MSGPACK_DEFINE_STRUCT(StandardSignInRequest,
                          MSGPACK_REQUEST_ARGS, params);
};

struct StandardSignInResponse : Response {
    struct Result {
        std::string jwt_challenge;
        MSGPACK_DEFINE(jwt_challenge);
    };

    Result result;
    MSGPACK_DEFINE_STRUCT(StandardSignInResponse,
                          MSGPACK_RESPONSE_ARGS, result);
};

struct StandardDidAuthRequest : Request {
    struct Params {
        std::string user_name;
        std::string jwt_vp;
        MSGPACK_DEFINE(user_name, jwt_vp);
    };

    Params params;
    MSGPACK_DEFINE_STRUCT(StandardDidAuthRequest,
                          MSGPACK_REQUEST_ARGS, params);
};

struct StandardDidAuthResponse : Response {
    struct Result {
        std::string access_token;
        MSGPACK_DEFINE(access_token);
    };

    Result result;
    MSGPACK_DEFINE_STRUCT(StandardDidAuthResponse,
                          MSGPACK_RESPONSE_ARGS, result);
};

struct GetMultiCommentsRequest : RequestWithToken {
    struct Params : RequestWithToken::Params {
        int64_t channel_id = -1;
        int64_t post_id = -1;
        int64_t by = -1;
        int64_t upper_bound = -1;
        int64_t lower_bound = -1;
        int64_t max_count = -1;
        MSGPACK_DEFINE(MSGPACK_REQUEST_TOKEN_ARGS,
                       channel_id, post_id, by, upper_bound, lower_bound, max_count);
    };

    Params params;
    MSGPACK_DEFINE_WITHTOKEN(GetMultiCommentsRequest, params.access_token,
                             MSGPACK_REQUEST_ARGS, params)
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
            MSGPACK_DEFINE(channel_id, post_id, comment_id, refer_comment_id,
                           status, user_did, user_name, content, likes, created_at, updated_at);
        };

        bool is_last = false;
        std::vector<Comment> comments;
        MSGPACK_DEFINE(is_last, comments);
    };

    Result result;
    MSGPACK_DEFINE_STRUCT(GetMultiCommentsResponse,
                          MSGPACK_RESPONSE_ARGS, result);
};

} // namespace Rpc
} // namespace trinity

#endif /* _FEEDS_RPC_DECLARE_HPP_ */


