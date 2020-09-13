
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