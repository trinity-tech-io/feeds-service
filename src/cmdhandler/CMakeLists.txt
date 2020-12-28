# pkg_search_module(pkg-openssl REQUIRED openssl)
# message(STATUS "openssl include path: ${pkg-openssl_INCLUDE_DIRS}")
# message(STATUS "openssl library path: ${pkg-openssl_STATIC_LDFLAGS}")

file( GLOB MASSDATA_SOURCES "*.cpp" )

if(${CMAKE_VERSION} VERSION_LESS "3.12")
    add_library(cmdhandler STATIC ${MASSDATA_SOURCES})
else()
    add_library(cmdhandler OBJECT ${MASSDATA_SOURCES})
endif()
add_dependencies(cmdhandler
    cvector
    libcrystal)

target_include_directories(cmdhandler PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(cmdhandler PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(cmdhandler PUBLIC
    cvector
    feedsd-ext
    libcrystal-shared
    libcarrier-shared
    utils)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
