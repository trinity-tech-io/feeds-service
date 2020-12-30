add_subdirectory(platform)

file( GLOB UTILS_SOURCES "*.c" "*.cpp")

if(${CMAKE_VERSION} VERSION_LESS "3.12")
    add_library(utils STATIC ${UTILS_SOURCES})
else()
    add_library(utils OBJECT ${UTILS_SOURCES})
endif()

target_include_directories(utils PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${FEEDS_INT_DIST_DIR}/include")

target_link_libraries(utils PUBLIC
    libcurl
    ssl
    crypto
    libcrystal-shared
    platform)
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    target_link_libraries(utils PUBLIC
    stdc++fs)
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
    target_link_libraries(utils PUBLIC
    log)
endif()

add_dependencies(utils
    libcrystal
    libressl
    libcurl)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
