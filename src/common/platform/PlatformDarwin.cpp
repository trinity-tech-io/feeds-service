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
