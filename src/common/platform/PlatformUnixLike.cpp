//
//  PlatformUnixLike.cpp
//
//  Created by mengxk on 19/03/16.
//  Copyright © 2016 mengxk. All rights reserved.
//

#if defined(__linux__) and not defined(__ANDROID__)

#include "PlatformUnixLike.hpp"

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

int PlatformUnixLike::GetCurrentDevId(std::string& devId)
{
    int ret = ErrCode::UnknownError;

    devId = "";

	long hostId = gethostid();
	std::stringstream hostIdStream;
	hostIdStream << std::hex << hostId;

    devId = hostIdStream.str();

    return 0;
}

int PlatformUnixLike::GetCurrentDevName(std::string& devName)
{
    devName = "";

    std::string uuidName;
    struct utsname utsName;
    int ret = uname(&utsName);
    if(ret < 0) {
        ret = ErrCode::DevUUIDError;
    }
    CHECK_ERROR(ret);
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

#endif /* __linux__ */
