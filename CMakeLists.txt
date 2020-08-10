cmake_minimum_required(VERSION 3.5)
cmake_policy(VERSION 3.5)
project(feedsd VERSION 1.0.0)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(FeedsDefaults)
include(AddSubModule)

add_subdirectory(deps)
add_subdirectory(src)
