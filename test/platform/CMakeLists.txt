file( GLOB PLATFORM_SOURCES "*.cpp" )

if(${CMAKE_VERSION} VERSION_LESS "3.12")
    add_library(platform STATIC ${PLATFORM_SOURCES})
else()
    add_library(platform OBJECT ${PLATFORM_SOURCES})
endif()
target_include_directories(platform PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include")
#target_link_libraries(platform common)
