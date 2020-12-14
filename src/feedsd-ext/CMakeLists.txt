# pkg_search_module(pkg-openssl REQUIRED openssl)
# message(STATUS "openssl include path: ${pkg-openssl_INCLUDE_DIRS}")
# message(STATUS "openssl library path: ${pkg-openssl_STATIC_LDFLAGS}")

file( GLOB MASSDATA_SOURCES "*.cpp" )

if(${CMAKE_VERSION} VERSION_LESS "3.12")
    add_library(feedsd-ext STATIC ${MASSDATA_SOURCES})
else()
    add_library(feedsd-ext OBJECT ${MASSDATA_SOURCES})
endif()
add_dependencies(feedsd-ext
    cvector
    libcrystal
    sqlitecpp-static)

target_include_directories(feedsd-ext PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(feedsd-ext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(feedsd-ext PUBLIC
    utils
    sqlitecpp-static)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
