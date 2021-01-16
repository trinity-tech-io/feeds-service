file( GLOB CLOUDDRIVE_SOURCES "*.cpp" )

if(${CMAKE_VERSION} VERSION_LESS "3.12")
    add_library(clouddrive STATIC ${CLOUDDRIVE_SOURCES})
else()
    add_library(clouddrive OBJECT ${CLOUDDRIVE_SOURCES})
endif()

target_include_directories(clouddrive PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(clouddrive PUBLIC
    utils)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)