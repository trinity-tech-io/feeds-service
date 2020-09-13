//
//  ErrCode.cpp
//
//  Created by mengxk on 19/03/16.
//  Copyright © 2016 mengxk. All rights reserved.
//

#include <ErrCode.hpp>

#include <system_error>

namespace elastos {

/***********************************************/
/***** static variables initialize *************/
/***********************************************/
std::function<void(int, const std::string&, const std::string&)> ErrCode::sErrorListener;


/***********************************************/
/***** static function implement ***************/
/***********************************************/
void ErrCode::SetErrorListener(std::function<void(int, const std::string&, const std::string&)> listener)
{
    sErrorListener = listener;
}

void ErrCode::SetError(int errCode, const std::string& ext)
{
    if(errCode >= 0) {
        return;
    }
    if(!sErrorListener) {
        return;
    }

    auto errStr = ToString(errCode);
    sErrorListener(errCode, errStr, ext);
}

std::string ErrCode::ToString(int errCode)
{
    std::string errMsg;

    switch (errCode) {
    case UnknownError:
        errMsg = "UnknownError";
        break;
    case InvalidArgument:
        errMsg = "InvalidArgument";
        break;
    case IOSystemException:
        errMsg = "IOSystemException";
        break;
    case NetworkException:
        errMsg = "NetworkException";
        break;
    case FileNotExistsError:
        errMsg = "FileNotExistsError";
        break;
    }

    if(errCode < StdSystemErrorIndex) { // is std error
        int stdErrVal = StdSystemErrorIndex - errCode;
        auto stdErrCode = std::error_code(stdErrVal, std::generic_category());
        errMsg = stdErrCode.message();
    }

  return errMsg;
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
