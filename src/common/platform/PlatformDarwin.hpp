/**
 * @file	PlatformDarwin.hpp
 * @brief	PlatformDarwin
 * @details	
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _ELASTOS_PLATFORM_DARWIN_HPP_
#define _ELASTOS_PLATFORM_DARWIN_HPP_

#if defined(__APPLE__)
#include "TargetConditionals.h"
#if !(TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR)

#include <string>

namespace elastos {

class PlatformDarwin {
public:
    /*** type define ***/

    /*** static function and variable ***/
    static void DetachCurrentThread() { /* NOUSE */ }

    static std::string GetBacktrace();
    static int GetCurrentDevId(std::string& devId);
    static int GetCurrentDevName(std::string& devName);

    static void SetCurrentDevId(const std::string& devId) { /* NOUSE */ }
    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit PlatformDarwin() = delete;
    virtual ~PlatformDarwin() = delete;

}; // class PlatformDarwin

} // namespace elastos

#endif // !(TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR)
#endif // defined(__APPLE__)

#endif /* _ELASTOS_PLATFORM_DARWIN_HPP_ */
