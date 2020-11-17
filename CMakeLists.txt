cmake_minimum_required(VERSION 3.5)
cmake_policy(VERSION 3.5)
project(feedsd VERSION 1.3.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(FeedsDefaults)
include(AddSubModule)
include(GetGitRevisionDescription)

get_git_head_revision(GIT_REFSPEC GIT_COMMIT_ID)
if(${GIT_COMMIT_ID} STREQUAL "GITDIR-NOTFOUND")
set(GIT_COMMIT_ID "0000000")
endif()
string(SUBSTRING ${GIT_COMMIT_ID} 0 7 GIT_COMMIT_ID)

add_subdirectory(deps)
add_subdirectory(src)

# if(DEFINED WITH_TEST)
# 	add_subdirectory(test)
# endif()
