include(CheckIncludeFile)
include(CheckFunctionExists)

check_include_file(alloca.h HAVE_ALLOCA_H)
if(HAVE_ALLOCA_H)
    add_definitions(-DHAVE_ALLOCA_H=1)
endif()

check_include_file(malloc.h HAVE_MALLOC_H)
if(HAVE_MALLOC_H)
    add_definitions(-DHAVE_MALLOC_H=1)
endif()

check_include_file(unistd.h HAVE_UNISTD_H)
if(HAVE_UNISTD_H)
    add_definitions(-DHAVE_UNISTD_H=1)
endif()

check_include_file(getopt.h HAVE_GETOPT_H)
if(HAVE_GETOPT_H)
    add_definitions(-DHAVE_GETOPT_H=1)
endif()

check_include_file(process.h HAVE_PROCESS_H)
if(HAVE_PROCESS_H)
    add_definitions(-DHAVE_PROCESS_H=1)
endif()

check_include_file(time.h HAVE_TIME_H)
if(HAVE_TIME_H)
    add_definitions(-DHAVE_TIME_H=1)
endif()

check_include_file(sys/resource.h HAVE_SYS_RESOURCE_H)
if(HAVE_SYS_RESOURCE_H)
    add_definitions(-DHAVE_SYS_RESOURCE_H=1)
endif()

check_include_file(sys/select.h HAVE_SYS_SELECT_H)
if(HAVE_SYS_SELECT_H)
    add_definitions(-DHAVE_SYS_SELECT_H=1)
endif()

check_include_file(direct.h HAVE_DIRECT_H)
if(HAVE_DIRECT_H)
    add_definitions(-DHAVE_DIRECT_H=1)
endif()

check_function_exists(sigaction HAVE_SIGACTION)
if(HAVE_SIGACTION)
    add_definitions(-DHAVE_SIGACTION=1)
endif()

message(STATUS "Configuring ver.h")
configure_file(ver.h.in ${CMAKE_CURRENT_BINARY_DIR}/gen/ver.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/gen)

link_directories(${FEEDS_INT_DIST_DIR}/lib)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(feedsd
    db.c
    cfg.c
    rpc.c
    err.c
    auth.c
    main.cpp
    msgq.cpp
    did.c
    feeds.c)

add_dependencies(feedsd
    Elastos.NET.Carrier.Native.SDK
    Elastos.DID.Native.SDK
    msgpack-c
    libcrystal
    libconfig
    libqrencode
    sqlitecpp-static
    cvector
    mkdirs
    sandbird)

target_link_libraries(feedsd
    msgpackc
    sqlite3
    config
    libcrystal-shared
    libcarrier-shared
    eladid
    libqrencode-static
    cvector
    mkdirs
    sandbird
    pthread
    dl
    m)

set(FEEDSD_LISTEN_IP 0.0.0.0)
configure_file(feedsd.conf.in feedsd.conf @ONLY
    NEWLINE_STYLE UNIX)

install(TARGETS feedsd
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    COMPONENT main)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/feedsd.conf
    DESTINATION etc/feedsd
    COMPONENT main)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    install(FILES ${CMAKE_SOURCE_DIR}/scripts/feedsd.service
        DESTINATION lib/systemd/system
        COMPONENT main)

    install(PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/feedsd.sh
        DESTINATION etc/init.d
        RENAME feedsd
        COMPONENT main)
endif()

add_subdirectory(cmdhandler)
add_subdirectory(feedsd-ext)
add_subdirectory(massdata)
add_subdirectory(utils)
target_link_libraries(feedsd
    cmdhandler
    feedsd-ext
    massdata
    platform
    utils)
