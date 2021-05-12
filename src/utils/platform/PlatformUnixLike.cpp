//
//  PlatformUnixLike.cpp
//
//  Created by mengxk on 19/03/16.
//  Copyright © 2016 mengxk. All rights reserved.
//

#if defined(__linux__) && !defined(__ANDROID__)

#include "PlatformUnixLike.hpp"

#include <execinfo.h>
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
