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
    std::string errMsg = "UnprocessedErrCode";

    switch (errCode) {
    case UnknownError:
        errMsg = "UnknownError";
        break;
    case UnimplementedError:
        errMsg = "UnimplementedError";
        break;
    case NotFoundError:
        errMsg = "NotFoundError";
        break;
    case InvalidArgument:
        errMsg = "InvalidArgument";
        break;
    case PointerReleasedError:
        errMsg = "PointerReleasedError";
        break;
    case DevUUIDError:
        errMsg = "DevUUIDError";
        break;
    case FileNotExistsError:
        errMsg = "FileNotExistsError";
        break;
    case CreateDirectoryError:
        errMsg = "CreateDirectoryError";
        break;
    case SizeOverflowError:
        errMsg = "SizeOverflowError";
        break;
    case StdSystemError:
        errMsg = "StdSystemError";
        break;
    case DidNotReady:
        errMsg = "DidNotReady";
        break;
    case InvalidAccessToken:
        errMsg = "InvalidAccessToken";
        break;
    case NotAuthorizedError:
        errMsg = "NotAuthorizedError";
        break;
    case CarrierSessionInitFailed:
        errMsg = "CarrierSessionInitFailed";
        break;
    case CarrierSessionConnectFailed:
        errMsg = "CarrierSessionConnectFailed";
        break;
    case CarrierSessionCreateFailed:
        errMsg = "CarrierSessionCreateFailed";
        break;
    case CarrierSessionAddStreamFailed:
        errMsg = "CarrierSessionAddStreamFailed";
        break;
    case CarrierSessionTimeoutError:
        errMsg = "CarrierSessionTimeoutError";
        break;
    case CarrierSessionReplyFailed:
        errMsg = "CarrierSessionReplyFailed";
        break;
    case CarrierSessionStartFailed:
        errMsg = "CarrierSessionStartFailed";
        break;
    case CarrierSessionBadStatus:
        errMsg = "CarrierSessionBadStatus";
        break;
    case CarrierSessionDataNotEnough:
        errMsg = "CarrierSessionDataNotEnough";
        break;
    case CarrierSessionUnsuppertedVersion:
        errMsg = "CarrierSessionUnsuppertedVersion";
        break;
    case CarrierSessionReleasedError:
        errMsg = "CarrierSessionReleasedError";
        break;
    case CarrierSessionSendFailed:
        errMsg = "CarrierSessionSendFailed";
        break;
    case CarrierSessionErrorExists:
        errMsg = "CarrierSessionErrorExists";
        break;
    case MassDataUnknownReqFailed:
        errMsg = "MassDataUnknownReqFailed";
        break;
    case MassDataUnmarshalReqFailed:
        errMsg = "MassDataUnmarshalReqFailed";
        break;
    case MassDataMarshalRespFailed:
        errMsg = "MassDataMarshalRespFailed";
        break;
    case MassDataUnsupportedAlgo:
        errMsg = "MassDataUnsupportedAlgo";
        break;
    case MassDataUnsupportedVersion:
        errMsg = "MassDataUnsupportedVersion";
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
