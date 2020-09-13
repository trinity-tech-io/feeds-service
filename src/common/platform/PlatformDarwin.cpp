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
#include <sys/utsname.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include "ErrCode.hpp"
#include "Log.hpp"

namespace elastos {

/***********************************************/
/***** static variables initialize *************/
/***********************************************/


/***********************************************/
/***** static function implement ***************/
/***********************************************/
std::string PlatformDarwin::GetBacktrace() {
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

int PlatformDarwin::GetCurrentDevId(std::string& devId)
{
    int ret = ErrCode::UnknownError;

    devId = "";

    uuid_t uuid = {};
    struct timespec ts = { .tv_sec = 5, .tv_nsec = 0 };
   ret = gethostuuid(uuid, &ts);
    if(ret < 0) {
        return ErrCode::DevUUIDError;
    }

    uuid_string_t uuidStr;
    uuid_unparse_upper(uuid, uuidStr);
    devId = uuidStr;
    // devId += "aaa";

    return 0;
}

int PlatformDarwin::GetCurrentDevName(std::string& devName)
{
    int ret = ErrCode::UnknownError;

    devName = "";

    struct utsname utsName;
    ret = uname(&utsName);
    if(ret < 0) {
        return ErrCode::DevUUIDError;
    }
    devName = utsName.sysname;

    return 0;
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

} // namespace elastos

#endif // !defined(TARGET_OS_IOS)
#endif // defined(__APPLE__)
