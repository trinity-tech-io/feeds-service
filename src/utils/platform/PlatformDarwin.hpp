/**
 * @file	PlatformDarwin.hpp
 * @brief	PlatformDarwin
 * @details	
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _TRINITY_PLATFORM_DARWIN_HPP_
#define _TRINITY_PLATFORM_DARWIN_HPP_

#if defined(__APPLE__)
#include "TargetConditionals.h"
#if !(TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR)

#include <string>
#include <StdFileSystem.hpp>

namespace trinity {

class PlatformDarwin {
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
    explicit PlatformDarwin() = delete;
    virtual ~PlatformDarwin() = delete;

}; // class PlatformDarwin

} // namespace trinity

#endif // !(TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR)
#endif // defined(__APPLE__)

#endif /* _TRINITY_PLATFORM_DARWIN_HPP_ */
