# pkg_search_module(pkg-openssl REQUIRED openssl)
# message(STATUS "openssl include path: ${pkg-openssl_INCLUDE_DIRS}")
# message(STATUS "openssl library path: ${pkg-openssl_STATIC_LDFLAGS}")

add_subdirectory(platform)

file( GLOB UTILS_SOURCES "*.c" "*.cpp")

if(${CMAKE_VERSION} VERSION_LESS "3.12")
    add_library(utils STATIC ${UTILS_SOURCES})
else()
    add_library(utils OBJECT ${UTILS_SOURCES})
endif()

add_dependencies(utils
    libcrystal
    libcurl)

target_include_directories(utils PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty"
    "${pkg-openssl_INCLUDE_DIRS}"
    "${FEEDS_INT_DIST_DIR}/include")

target_link_libraries(utils PUBLIC
    platform
    libcurl)
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    target_link_libraries(utils PUBLIC
    stdc++fs)
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
    target_link_libraries(utils PUBLIC
    log)
endif()


set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
