#ifndef _FEEDS_SAFE_PTR_HPP_
#define _FEEDS_SAFE_PTR_HPP_

#include "ErrCode.hpp"
#include "Log.hpp"

#define SAFE_GET_PTR(var, weakptr)                                                                       \
    auto var = weakptr.lock();                                                                      \
    if(var.get() == nullptr) {                                                                      \
        Log::E(Log::Tag::Err, "Failed to get saft ptr from " #weakptr " at %s(%d)", __FILE__, __LINE__); \
        return trinity::ErrCode::PointerReleasedError;                                                       \
    }

#define SAFE_GET_PTR_NO_RETVAL(var, weakptr)                                                             \
    auto var = weakptr.lock();                                                                      \
    if(var.get() == nullptr) {                                                                      \
        Log::E(Log::Tag::Err, "Failed to get saft ptr from " #weakptr " at %s(%d)", __FILE__, __LINE__); \
        return;                                                                                     \
    }

#define SAFE_GET_PTR_DEF_RETVAL(var, weakptr, retval)                                                   \
    auto var = weakptr.lock();                                                                     \
    if(var.get() == nullptr) {                                                                     \
        Log::E(Log::Tag::Err, "Failed to get saft ptr from " #weakptr " at %s(%d)", __FILE__, __LINE__); \
        return retval;                                                                                     \
    }

#define SAFE_GET_PTR(var, weakptr)                                                                       \
    auto var = weakptr.lock();                                                                      \
    if(var.get() == nullptr) {                                                                      \
        Log::E(Log::Tag::Err, "Failed to get saft ptr from " #weakptr " at %s(%d)", __FILE__, __LINE__); \
        return trinity::ErrCode::PointerReleasedError;                                                       \
    }

#if defined(__APPLE__)
namespace std {
template <class T, class U>
static std::shared_ptr<T> reinterpret_pointer_cast(const std::shared_ptr<U> &r) noexcept
{
    auto p = reinterpret_cast<typename std::shared_ptr<T>::element_type *>(r.get());
    return std::shared_ptr<T>(r, p);
}
} // namespace std
#endif // defined(__APPLE__)

#endif /* _FEEDS_SAFE_PTR_HPP_ */
