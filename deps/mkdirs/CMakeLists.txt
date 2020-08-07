project(mkdirs)

add_library(mkdirs STATIC mkdirs.c)
target_include_directories(mkdirs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})