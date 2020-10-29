#ifndef _FEEDS_SAFE_PTR_HPP_
#define _FEEDS_SAFE_PTR_HPP_

#include "ErrCode.hpp"
#include "Log.hpp"

#define SAFE_GET_PTR(weakptr)                                                                       \
({                                                                                                  \
    auto ptr = weakptr.lock();                                                                      \
    if(ptr.get() == nullptr) {                                                                      \
        Log::E(Log::TAG, "Failed to get saft ptr from " #weakptr " at %s(%d)", __FILE__, __LINE__); \
        return trinity::ErrCode::PointerReleasedError;                                                       \
    }                                                                                               \
    ptr;                                                                                            \
})

#define SAFE_GET_PTR_NO_RETVAL(weakptr)                                                             \
({                                                                                                  \
    auto ptr = weakptr.lock();                                                                      \
    if(ptr.get() == nullptr) {                                                                      \
        Log::E(Log::TAG, "Failed to get saft ptr from " #weakptr " at %s(%d)", __FILE__, __LINE__); \
        return;                                                                                     \
    }                                                                                               \
    ptr;                                                                                            \
})

#define SAFE_GET_PTR_DEF_RETVAL(weakptr, retval)                                                            \
({                                                                                                  \
    auto ptr = weakptr.lock();                                                                      \
    if(ptr.get() == nullptr) {                                                                      \
        Log::E(Log::TAG, "Failed to get saft ptr from " #weakptr " at %s(%d)", __FILE__, __LINE__); \
        return retval;                                                                                     \
    }                                                                                               \
    ptr;                                                                                            \
})

#define SAFE_GET_PTR(weakptr)                                                                       \
({                                                                                                  \
    auto ptr = weakptr.lock();                                                                      \
    if(ptr.get() == nullptr) {                                                                      \
        Log::E(Log::TAG, "Failed to get saft ptr from " #weakptr " at %s(%d)", __FILE__, __LINE__); \
        return trinity::ErrCode::PointerReleasedError;                                                       \
    }                                                                                               \
    ptr;                                                                                            \
})

#endif /* _FEEDS_SAFE_PTR_HPP_ */
