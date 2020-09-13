/**
 * @file	PlatformIos.hpp
 * @brief	PlatformIos
 * @details	
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _ELASTOS_PLATFORM_IOS_HPP_
#define _ELASTOS_PLATFORM_IOS_HPP_

#if defined(__APPLE__)
#include "TargetConditionals.h"
#if defined(TARGET_OS_IOS)

#include <string>

namespace elastos {

class PlatformIos {
public:
    /*** type define ***/

    /*** static function and variable ***/
    static void DetachCurrentThread() { /* NOUSE */ }

    static std::string GetBacktrace();
    static int GetCurrentDevId(std::string& devId);
    static int GetCurrentDevName(std::string& devName);

    static void SetCurrentDevId(const std::string& devId);
    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit PlatformIos() = delete;
    virtual ~PlatformIos() = delete;

    static std::string mCurrentDevId;

}; // class PlatformIos

} // namespace elastos

#endif // defined(TARGET_OS_IOS)
#endif // defined(__APPLE__)

#endif /* _ELASTOS_PLATFORM_IOS_HPP_ */
