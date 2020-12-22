//
//  PlatformDarwin.cpp
//
//  Created by mengxk on 19/03/16.
//  Copyright © 2016 mengxk. All rights reserved.
//

#if defined(__APPLE__)
#include "TargetConditionals.h"
#if !(TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR)

#include "PlatformDarwin.hpp"

#include <execinfo.h>
#include <sstream>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include "ErrCode.hpp"
#include "Log.hpp"

namespace trinity {

/***********************************************/
/***** static variables initialize *************/
/***********************************************/


/***********************************************/
/***** static function implement ***************/
/***********************************************/
std::string PlatformDarwin::GetProductName()
{
    return "MacOSX";
}

std::string PlatformDarwin::GetProductVersion()
{
    char version[256];
    size_t size = sizeof(version);
    int ret = sysctlbyname("kern.osproductversion", version, &size, NULL, 0);
    if(ret < 0) {
        Log::E(Log::Tag::Util, "Failed to get product version, errno=%d", errno);
        return "0";
    }

    return version;
}

int PlatformDarwin::UnpackUpgradeTarball(const std::filesystem::path& from,
                                         const std::filesystem::path& to)
{
    return -1;
}

std::string PlatformDarwin::GetBacktrace() {
    void* addrlist[512];
    int addrlen = backtrace( addrlist, sizeof( addrlist ) / sizeof( void* ));
    if(addrlen == 0) {
        return "no backtrace.";
    }

    std::stringstream sstream;
    char** symbollist = backtrace_symbols( addrlist, addrlen );
    for ( int i = 0; i < addrlen; i++ ) {
        sstream << symbollist[i] << std::endl;
    }
    free(symbollist);

    return sstream.str();
}

/***********************************************/
/***** class public function implement  ********/
/***********************************************/

/***********************************************/
/***** class protected function implement  *****/
/***********************************************/


/***********************************************/
/***** class private function implement  *******/
/***********************************************/

} // namespace trinity

#endif // !defined(TARGET_OS_IOS)
#endif // defined(__APPLE__)
