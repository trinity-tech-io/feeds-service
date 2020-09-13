/**
 * @file	Random.hpp
 * @brief	Random
 * @details	
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _ELASTOS_RANDOM_HPP_
#define _ELASTOS_RANDOM_HPP_

#include <random>

namespace elastos {

class Random final {
public:
    /*** type define ***/

    /*** static function and variable ***/
    template<class T>
    static T Gen() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<T> dis {};
        T rand = dis(gen);
        return rand;
    }

    template<class T>
    static T Gen(T range) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<T> dis {0, range};
        T rand = dis(gen);
        return rand;
    }

    /*** class function and variable ***/


protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit Random() = delete;
    virtual ~Random() = delete;

}; // class Random

} // namespace elastos

#endif /* _ELASTOS_RANDOM_HPP_ */
