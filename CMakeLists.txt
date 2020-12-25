cmake_minimum_required(VERSION 3.5)
cmake_policy(VERSION 3.5)
project(feedsd VERSION 1.5.0)
math(EXPR PROJECT_VERSION_CODE
         " ${PROJECT_VERSION_MAJOR} * 10000
         + ${PROJECT_VERSION_MINOR} * 100
         + ${PROJECT_VERSION_PATCH}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(FEEDS_INT_RUNTIME_DIR var/lib/feedsd/runtime)
set(FEEDS_INT_ROOT_DIR ${FEEDS_INT_RUNTIME_DIR}/${PROJECT_VERSION_CODE})
include(FeedsDefaults)
include(AddSubModule)
include(GitCommitId)

git_head_commit_id(GIT_COMMIT_ID)

add_subdirectory(deps)
add_subdirectory(src)

# if(DEFINED WITH_TEST)
# 	add_subdirectory(test)
# endif()
