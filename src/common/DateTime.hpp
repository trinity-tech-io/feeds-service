/**
 * @file	DateTime.hpp
 * @brief	DateTime
 * @details	
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _ELASTOS_DATETIME_HPP_
#define _ELASTOS_DATETIME_HPP_

#include <chrono>
#include <string>
#include <memory>
#include <ctime>

namespace elastos {

using std::chrono::system_clock;

class DateTime final {
public:
    /*** type define ***/

    /*** static function and variable ***/
    static std::string Current() {
        system_clock::time_point tp = system_clock::now();
        time_t raw_time = system_clock::to_time_t(tp);
        struct tm *timeinfo  = std::localtime(&raw_time);

        char buf[24] = {0};
        strftime(buf, 24, "%Y-%m-%d %H:%M:%S,", timeinfo);

        std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
        std::string milliseconds_str =  std::to_string(ms.count() % 1000);
        if(milliseconds_str.length() < 3) {
            milliseconds_str = std::string(3 - milliseconds_str.length(), '0') + milliseconds_str;
        }

        return std::string(buf) + milliseconds_str;
    }

    static int64_t CurrentMS() {
        system_clock::time_point tp = system_clock::now();
        std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
        return now.count();
    }

    /*** class function and variable ***/


protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit DateTime() = delete;
    virtual ~DateTime() = delete;

}; // class DateTime

} // namespace elastos

#endif /* _ELASTOS_DATETIME_HPP_ */
