/**
 * @file	PlatformUnixLike.hpp
 * @brief	PlatformUnixLike
 * @details	
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _TRINITY_PLATFORM_UNIXLIKE_HPP_
#define _TRINITY_PLATFORM_UNIXLIKE_HPP_

#if defined(__linux__) and not defined(__ANDROID__)

#include <string>

namespace trinity {

class PlatformUnixLike {
public:
    /*** type define ***/

    /*** static function and variable ***/
    static std::string GetProductName();
    static std::string GetProductVersion();

    static int UnpackUpgradeTarball(const std::filesystem::path& from,
                                    const std::filesystem::path& to);

    static std::string GetBacktrace();
    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit PlatformUnixLike() = delete;
    virtual ~PlatformUnixLike() = delete;

}; // class PlatformUnixLike

} // namespace trinity

#endif /* __linux__ */

#endif /* _TRINITY_PLATFORM_UNIXLIKE_HPP_ */

