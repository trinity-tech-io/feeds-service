# pkg_search_module(pkg-openssl REQUIRED openssl)
# message(STATUS "openssl include path: ${pkg-openssl_INCLUDE_DIRS}")
# message(STATUS "openssl library path: ${pkg-openssl_STATIC_LDFLAGS}")

if(${CMAKE_VERSION} VERSION_LESS "3.12")
    add_library(autoupdate STATIC AutoUpdate.cpp)
else()
    add_library(autoupdate OBJECT AutoUpdate.cpp)
endif()

target_include_directories(autoupdate PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(autoupdate PUBLIC
    cvector
    utils)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

add_executable(feedsd.updatehelper
    AutoUpdateHelper.cpp)

target_link_libraries(feedsd.updatehelper PUBLIC
    platform
    utils)

install(TARGETS feedsd.updatehelper
    RUNTIME DESTINATION ${FEEDS_INT_ROOT_DIR}/bin
    COMPONENT main)