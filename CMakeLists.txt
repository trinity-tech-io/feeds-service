cmake_minimum_required(VERSION 3.5)
cmake_policy(VERSION 3.5)
project(feedsd VERSION 2.0.1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(FeedsDefaults)
include(AddSubModule)
include(GitCommitId)

git_head_commit_id(GIT_COMMIT_ID)

add_subdirectory(deps)
add_subdirectory(src)

# if(DEFINED WITH_TEST)
# 	add_subdirectory(test)
# endif()
