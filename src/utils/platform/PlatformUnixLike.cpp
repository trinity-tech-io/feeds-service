//
//  PlatformUnixLike.cpp
//
//  Created by mengxk on 19/03/16.
//  Copyright © 2016 mengxk. All rights reserved.
//

#if defined(__linux__) and not defined(__ANDROID__)

#include "PlatformUnixLike.hpp"

#include <execinfo.h>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <sys/utsname.h>
#include <unistd.h>
#include "ErrCode.hpp"
#include "Log.hpp"

namespace trinity {

/***********************************************/
/***** static variables initialize *************/
/***********************************************/


/***********************************************/
/***** static function implement ***************/
/***********************************************/
std::string PlatformUnixLike::GetProductName()
{
    std::ifstream releaseFile("/etc/lsb-release");
    std::stringstream content;
    content << releaseFile.rdbuf();

    std::string result = content.str();
    result = std::regex_replace(result, std::regex("\n"), " ");
    result = std::regex_replace(result, std::regex(".*DISTRIB_ID="), "");
    result = std::regex_replace(result, std::regex(" .*"), "");

    return result;
}

std::string PlatformUnixLike::GetProductVersion()
{
    std::ifstream releaseFile("/etc/lsb-release");
    std::stringstream content;
    content << releaseFile.rdbuf();

    std::string result = content.str();
    result = std::regex_replace(result, std::regex("\n"), " ");
    result = std::regex_replace(result, std::regex(".*DISTRIB_RELEASE="), "");
    result = std::regex_replace(result, std::regex(" .*"), "");

    return result;
}

int PlatformUnixLike::UnpackUpgradeTarball(const std::filesystem::path& from,
                                           const std::filesystem::path& to)
{
    return -1;
}

std::string PlatformUnixLike::GetBacktrace() {
    void* addrlist[512];
    int addrlen = backtrace( addrlist, sizeof( addrlist ) / sizeof( void* ));
    if(addrlen == 0) {
        return "no backtrace.";
    }

    std::stringstream sstream;
    char** symbollist = backtrace_symbols( addrlist, addrlen );
    for ( int i = 4; i < addrlen; i++ ) {
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

#endif /* __linux__ */
