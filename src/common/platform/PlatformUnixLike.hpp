/**
 * @file	PlatformUnixLike.hpp
 * @brief	PlatformUnixLike
 * @details	
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _ELASTOS_PLATFORM_UNIXLIKE_HPP_
#define _ELASTOS_PLATFORM_UNIXLIKE_HPP_

#if defined(__linux__) and not defined(__ANDROID__)

#include <string>

namespace elastos {

class PlatformUnixLike {
public:
    /*** type define ***/

    /*** static function and variable ***/
    static std::string GetBacktrace();
    static int GetCurrentDevId(std::string& devId);
    static int GetCurrentDevName(std::string& devName);

    static void SetCurrentDevId(const std::string& devId) { /* NOUSE */ }
    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit PlatformUnixLike() = delete;
    virtual ~PlatformUnixLike() = delete;

}; // class PlatformUnixLike

} // namespace elastos

#endif /* __linux__ */

#endif /* _ELASTOS_PLATFORM_UNIXLIKE_HPP_ */

