/**
 * @file	StdFileSystem.hpp
 * @brief	StdFileSystem
 * @details	
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _STD_FILESYSTEM_HPP_
#define _STD_FILESYSTEM_HPP_

#if(defined(__linux__) and (__GNUC__ < 8))
#include <experimental/filesystem>
namespace std {
    namespace filesystem = std::experimental::filesystem;
}
#else
#include <filesystem>
#endif

#endif /* _STD_FILESYSTEM_HPP_ */
