project(sandbird)

add_library(sandbird STATIC sandbird.c)
target_include_directories(sandbird PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})