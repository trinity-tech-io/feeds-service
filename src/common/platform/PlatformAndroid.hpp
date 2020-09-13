/**
 * @file	PlatformAndroid.hpp
 * @brief	PlatformAndroid
 * @details	
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _ELASTOS_PLATFORM_ANDROID_HPP_
#define _ELASTOS_PLATFORM_ANDROID_HPP_

#if defined(__ANDROID__)

#include <string>
#include <unwind.h>

namespace elastos {

class PlatformAndroid {
public:
    /*** type define ***/

    /*** static function and variable ***/
    static void SetJavaVM(void* jvm);
//    static void DetachCurrentThread();
    static bool CallOnload(bool(*func)(void*, void*));

    static std::string GetBacktrace();
    static int GetCurrentDevId(std::string& devId);
    static int GetCurrentDevName(std::string& devName);

    static void SetCurrentDevId(const std::string& devId);
    /*** class function and variable ***/

private:
    /*** type define ***/
    struct BacktraceState {
        void** current;
        void** end;
    };

    /*** static function and variable ***/
    static _Unwind_Reason_Code UnwindCallback(struct _Unwind_Context* context, void* arg);

    /*** class function and variable ***/
    explicit PlatformAndroid() = delete;
    virtual ~PlatformAndroid() = delete;

    static std::string mCurrentDevId;
    static void* mJVM;

}; // class PlatformAndroid

} // namespace elastos

#endif /* defined(__ANDROID__) */

#endif /* _ELASTOS_PLATFORM_ANDROID_HPP_ */
