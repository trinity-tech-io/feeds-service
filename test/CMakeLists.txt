# set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_subdirectory(common)
add_subdirectory(platform)

add_executable(feeds-cli)
set_target_properties(feeds-cli PROPERTIES LINK_FLAGS
	"-Wall" )
file( GLOB feeds-cli-SOURCES "*.cpp")
target_sources(feeds-cli PRIVATE
    ${feeds-cli-SOURCES})
target_include_directories(feeds-cli PRIVATE
    "${CMAKE_INSTALL_PREFIX}/include")
target_link_libraries(feeds-cli PRIVATE
    common
    platform)

install(TARGETS feeds-cli
    RUNTIME DESTINATION bin
    COMPONENT main)