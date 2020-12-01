# pkg_search_module(pkg-openssl REQUIRED openssl)
# message(STATUS "openssl include path: ${pkg-openssl_INCLUDE_DIRS}")
# message(STATUS "openssl library path: ${pkg-openssl_STATIC_LDFLAGS}")

file( GLOB MASSDATA_SOURCES "*.cpp" )

if(${CMAKE_VERSION} VERSION_LESS "3.12")
    add_library(utils STATIC ${MASSDATA_SOURCES})
else()
    add_library(utils OBJECT ${MASSDATA_SOURCES})
endif()
add_dependencies(utils
    cvector
    libcrystal
    sqlitecpp-static)

target_include_directories(utils PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(utils PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(utils PUBLIC
    common
    sqlitecpp-static)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
