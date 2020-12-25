#ifndef _FEEDS_RPC_DECLARE_HPP_
#define _FEEDS_RPC_DECLARE_HPP_

#include <MsgPackExtension.hpp>

namespace trinity {
namespace Rpc {

struct Base {
    virtual void pack(msgpack::sbuffer& buf) = 0;
    virtual void unpack(const msgpack::object& obj) = 0;
    virtual std::string str() = 0;
    DECLARE_DEFAULT_STRUCT(Base);
};

struct Request : Base {
    std::string version;
    std::string method;
    int64_t id = -1;
    MSGPACK_DEFINE_STRUCT(Request, MSGPACK_REQUEST_ARGS);
};

struct Response : Base {
    std::string version;
    int64_t id = -1;
    MSGPACK_DEFINE_STRUCT(Response, MSGPACK_RESPONSE_ARGS);
};

struct Notify : Base {
    std::string version;
    std::string method;
    MSGPACK_DEFINE_STRUCT(Notify, MSGPACK_NOTIFY_ARGS);
};

struct Error : Response {
    struct Detail {
        int64_t code;
        std::string message;
        MSGPACK_DEFINE(code, message);
    };

    Detail error;
    MSGPACK_DEFINE_STRUCT(Error, MSGPACK_RESPONSE_ARGS, error);
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

struct GetMultiLikesAndCommentsCountRequest : RequestWithToken {
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
    MSGPACK_DEFINE_WITHTOKEN(GetMultiLikesAndCommentsCountRequest, params.access_token,
                             MSGPACK_REQUEST_ARGS, params)
};

struct GetMultiLikesAndCommentsCountResponse : Response {
    struct Result {
        struct Post {
            int64_t channel_id = -1;
            int64_t post_id = -1;
            int64_t comments_count = -1;
            int64_t likes_count = -1;

            MSGPACK_DEFINE(channel_id, post_id, comments_count, likes_count);
        };

        bool is_last = false;
        std::vector<Post> posts;
        MSGPACK_DEFINE(is_last, posts);
    };

    Result result;
    MSGPACK_DEFINE_STRUCT(GetMultiLikesAndCommentsCountResponse,
                          MSGPACK_RESPONSE_ARGS, result);
};

struct GetMultiSubscribersCountRequest : RequestWithToken {
    struct Params : RequestWithToken::Params {
        int64_t channel_id = -1;
        MSGPACK_DEFINE(MSGPACK_REQUEST_TOKEN_ARGS, channel_id);
    };

    Params params;
    MSGPACK_DEFINE_WITHTOKEN(GetMultiSubscribersCountRequest, params.access_token,
                             MSGPACK_REQUEST_ARGS, params)
};

struct GetMultiSubscribersCountResponse : Response {
    struct Result {
        struct Channel {
            int64_t channel_id = -1;
            int64_t subscribers_count = -1;

            MSGPACK_DEFINE(channel_id, subscribers_count);
        };

        bool is_last = false;
        std::vector<Channel> channels;
        MSGPACK_DEFINE(is_last, channels);
    };

    Result result;
    MSGPACK_DEFINE_STRUCT(GetMultiSubscribersCountResponse,
                          MSGPACK_RESPONSE_ARGS, result);
};

struct DownloadNewServiceRequest : RequestWithToken {
    struct Params : RequestWithToken::Params {
        struct Tarball {
            std::string name;
            int64_t size;
            std::string md5;
            MSGPACK_DEFINE(name, size, md5);
        };
        
        int64_t new_version_code = -1;
        std::string base_url;
        Tarball macosx;
        Tarball ubuntu_1804;
        Tarball ubuntu_2004;
        Tarball raspberrypi;
        MSGPACK_DEFINE(MSGPACK_REQUEST_TOKEN_ARGS, new_version_code,
                       base_url, macosx, ubuntu_1804, ubuntu_2004, raspberrypi);
    };

    Params params;
    MSGPACK_DEFINE_WITHTOKEN(DownloadNewServiceRequest, params.access_token,
                             MSGPACK_REQUEST_ARGS, params)
};

struct DownloadNewServiceResponse : Response {
    MSGPACK_DEFINE_STRUCT(DownloadNewServiceResponse,
                          MSGPACK_RESPONSE_ARGS);
};


struct DownloadNewServiceNotify : Notify {
    MSGPACK_DEFINE_STRUCT(DownloadNewServiceNotify,
                          MSGPACK_NOTIFY_ARGS);
};

struct StartNewServiceRequest : RequestWithToken {
    struct Params : RequestWithToken::Params {
        int64_t new_version_code = -1;
        MSGPACK_DEFINE(MSGPACK_REQUEST_TOKEN_ARGS, new_version_code);
    };

    Params params;
    MSGPACK_DEFINE_WITHTOKEN(StartNewServiceRequest, params.access_token,
                             MSGPACK_REQUEST_ARGS, params)
};

struct StartNewServiceResponse : Response {
    MSGPACK_DEFINE_STRUCT(StartNewServiceResponse,
                          MSGPACK_RESPONSE_ARGS);
};

struct StartNewServiceNotify : Notify {
    MSGPACK_DEFINE_STRUCT(StartNewServiceNotify,
                          MSGPACK_NOTIFY_ARGS);
};

} // namespace Rpc
} // namespace trinity

#endif /* _FEEDS_RPC_DECLARE_HPP_ */


