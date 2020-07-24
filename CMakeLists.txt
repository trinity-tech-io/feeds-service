cmake_minimum_required(VERSION 3.5)
cmake_policy(VERSION 3.5)
project(elastos-feedsd
    VERSION 0.1.0)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(FeedsDefaults)
include(AddSubModule)

set(ENABLE_TESTS ON CACHE BOOL "Build test cases")

add_subdirectory(deps)
add_subdirectory(src)

#if(ENABLE_TESTS)
#    add_subdirectory(tests)
#endif()
