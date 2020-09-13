//
//  PlatformIos.cpp
//
//  Created by mengxk on 19/03/16.
//  Copyright © 2016 mengxk. All rights reserved.
//

#if defined(__APPLE__)
#include "TargetConditionals.h"
#if defined(TARGET_OS_IOS)

#include "PlatformIos.hpp"

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
std::string PlatformIos::mCurrentDevId;

/***********************************************/
/***** static function implement ***************/
/***********************************************/
std::string PlatformIos::GetBacktrace() {
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

int PlatformIos::GetCurrentDevId(std::string& devId)
{
    if(mCurrentDevId.empty()) {
        return ErrCode::DevUUIDError;
    }

    devId = mCurrentDevId;
    return 0;
}

int PlatformIos::GetCurrentDevName(std::string& devName)
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

void PlatformIos::SetCurrentDevId(const std::string& devId)
{
    mCurrentDevId = devId;
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

#endif // defined(TARGET_OS_IOS)
#endif // defined(__APPLE__)
