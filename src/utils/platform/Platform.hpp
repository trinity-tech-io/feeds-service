/**
 * @file	PlatformDarwin.hpp
 * @brief	PlatformDarwin
 * @details
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _TRINITY_PLATFORM_HPP_
#define _TRINITY_PLATFORM_HPP_

#include "Log.hpp"

#if defined(__APPLE__)
#include "TargetConditionals.h"
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#include "PlatformIos.hpp"
#elif TARGET_OS_OSX
#include "PlatformDarwin.hpp"
#else
#error "Unsupport Apple Platform"
#endif

#elif defined(__ANDROID__)
#include "PlatformAndroid.hpp"
#elif defined(__linux__)
#include "PlatformUnixLike.hpp"
#else
//#error "Unsupport Platform"
#endif

namespace trinity {

#if defined(__APPLE__)
#if TARGET_OS_OSX
class Platform : public PlatformDarwin {
#endif
#elif defined(__linux__)
class Platform : public PlatformUnixLike {
#else
class Platform {
#endif
public:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit Platform() = delete;
    virtual ~Platform() = delete;

}; // class PlatformDarwin

} // namespace trinity

#endif /* _TRINITY_PLATFORM_HPP_ */
