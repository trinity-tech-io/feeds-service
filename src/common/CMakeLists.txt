# pkg_search_module(pkg-openssl REQUIRED openssl)
# message(STATUS "openssl include path: ${pkg-openssl_INCLUDE_DIRS}")
# message(STATUS "openssl library path: ${pkg-openssl_STATIC_LDFLAGS}")

add_subdirectory(platform)

file( GLOB COMMON_SOURCES "*.c" "*.cpp")

if(${CMAKE_VERSION} VERSION_LESS "3.12")
    add_library(common STATIC ${COMMON_SOURCES})
else()
    add_library(common OBJECT ${COMMON_SOURCES})
endif()

add_dependencies(common
    libcrystal)

target_include_directories(common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${pkg-openssl_INCLUDE_DIRS}"
    "${FEEDS_INT_DIST_DIR}/include")

target_link_libraries(common PUBLIC
    platform)
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    target_link_libraries(common PUBLIC
    stdc++fs)
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
    target_link_libraries(common PUBLIC
    log)
endif()


set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
