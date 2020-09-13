/**
 * @file	PlatformDarwin.hpp
 * @brief	PlatformDarwin
 * @details	
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _ELASTOS_PLATFORM_HPP_
#define _ELASTOS_PLATFORM_HPP_

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
#error "Unsupport Platform"
#endif

namespace elastos {

#if defined(__APPLE__)
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
class Platform : public PlatformIos {
#elif TARGET_OS_OSX
class Platform : public PlatformDarwin {
#endif
#elif defined(__ANDROID__)
class Platform : public PlatformAndroid {
#elif defined(__linux__)
class Platform : public PlatformUnixLike {
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

} // namespace elastos

#endif /* _ELASTOS_PLATFORM_HPP_ */
