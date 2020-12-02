#ifndef _FEEDS_MSGPACK_EXTENSION_HPP_
#define _FEEDS_MSGPACK_EXTENSION_HPP_

#include <msgpack.hpp>

#define MSGPACK_DEFINE_CLASS(ParentMsgPack, ParentMsgUnpack, ...) \
    virtual void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_pk) const \
    { \
        ParentMsgPack; \
        msgpack::type::make_define_map \
            MSGPACK_DEFINE_MAP_IMPL(__VA_ARGS__) \
            .msgpack_pack(msgpack_pk); \
    } \
    virtual void msgpack_unpack(msgpack::object const& msgpack_o) \
    { \
        ParentMsgUnpack; \
        msgpack::type::make_define_map \
            MSGPACK_DEFINE_MAP_IMPL(__VA_ARGS__) \
            .msgpack_unpack(msgpack_o); \
    }

#define MSGPACK_DEFINE_BASE(...) \
    MSGPACK_DEFINE_CLASS(;, ;, __VA_ARGS__)

#define MSGPACK_DEFINE_INHERIT(BaseClass, ...) \
    MSGPACK_DEFINE_CLASS( \
        BaseClass::msgpack_pack(msgpack_pk), \
        BaseClass::msgpack_unpack(msgpack_o), \
        __VA_ARGS__)

#endif /* _FEEDS_MSGPACK_EXTENSION_HPP_ */


