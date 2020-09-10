/**
 * @file	Base58.hpp
 * @brief	Base58
 * @details
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _ELASTOS_BASE58_HPP_
#define _ELASTOS_BASE58_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace elastos {

class Base58 {
public:
    /*** type define ***/
    //using Task = std::bind<F, Args...>;

    /*** static function and variable ***/
    static int Encode(const std::vector<uint8_t>& from,  std::string& to);
    static int Decode(const std::string& from, std::vector<uint8_t>& to);

    /*** class function and variable ***/

    // post and copy
    // post and move

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

}; // class Base58

} // namespace elastos

#endif /* _ELASTOS_BASE58_HPP_ */
