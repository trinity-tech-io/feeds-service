/**
 * @file	Base64.hpp
 * @brief	Base64
 * @details	
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _ELASTOS_BASE64_HPP_
#define _ELASTOS_BASE64_HPP_

namespace elastos {

class Base64 {
public:
    /*** type define ***/
    //using Task = std::bind<F, Args...>;

    /*** static function and variable ***/
    static int Encode(const std::vector<uint8_t>& from, std::string& to);
    static int Encode(const std::string& from, std::vector<uint8_t>& to);

    /*** class function and variable ***/

    // post and copy
    // post and move

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

}; // class Base64

} // namespace elastos

#endif /* _ELASTOS_BASE64_HPP_ */
