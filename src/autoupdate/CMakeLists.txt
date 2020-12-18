# pkg_search_module(pkg-openssl REQUIRED openssl)
# message(STATUS "openssl include path: ${pkg-openssl_INCLUDE_DIRS}")
# message(STATUS "openssl library path: ${pkg-openssl_STATIC_LDFLAGS}")

file( GLOB MASSDATA_SOURCES "*.cpp" )

if(${CMAKE_VERSION} VERSION_LESS "3.12")
    add_library(autoupdate STATIC ${MASSDATA_SOURCES})
else()
    add_library(autoupdate OBJECT ${MASSDATA_SOURCES})
endif()

target_include_directories(autoupdate PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(autoupdate PUBLIC
    utils)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
