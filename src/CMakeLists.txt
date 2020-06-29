include(FeedsDefaults)
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

set(SRC
    db.c
    cfg.c
    rpc.c
    err.c
    auth.c
    main.c
    mkdirs.c
    sandbird/sandbird.c
    did.c
    feeds.c)

include_directories(
    .
    ${FEEDS_INT_DIST_DIR}/include)

link_directories(
    ${FEEDS_INT_DIST_DIR}/lib)

set(LIBS
    msgpackc
    sqlite3
    config
    libcrystal-shared
    libelacarrier-shared
    libeladid-shared
    libqrencode-static
    pthread
    dl
    m)

set(ELAFEEDS_DEPENDS
    Elastos.NET.Carrier.Native.SDK
    Elastos.DID.Native.SDK
    msgpack-c
    libcrystal
    libconfig
    libqrencode
    sqlite)

add_executable(feeds_daemon ${SRC})
add_dependencies(feeds_daemon ${ELAFEEDS_DEPENDS})
set_target_properties(feeds_daemon PROPERTIES OUTPUT_NAME feedsd)
target_link_libraries(feeds_daemon ${LIBS})
install(TARGETS feeds_daemon
    RUNTIME DESTINATION "bin"
    ARCHIVE DESTINATION "lib"
    LIBRARY DESTINATION "lib")

install(FILES feeds.conf
    DESTINATION "etc/feeds")

set(SRC
    mkdirs.c
    rpc.c
    err.c
    feeds_client/cfg.c
    feeds_client/feeds_cli.c
    feeds_client/feeds_client.c)

set(FEEDSCLI_DEPENDS
    Elastos.NET.Carrier.Native.SDK
    Elastos.DID.Native.SDK
    libcrystal
    msgpack-c
    libconfig)

set(LIBS
    config
    msgpackc
    libcrystal-shared
    libelacarrier-shared
    libeladid-shared
    pthread)

add_executable(feeds_cli ${SRC})
add_dependencies(feeds_cli ${FEEDSCLI_DEPENDS})
set_target_properties(feeds_cli PROPERTIES OUTPUT_NAME feeds_cli)
target_link_libraries(feeds_cli ${LIBS})
install(TARGETS feeds_cli
    RUNTIME DESTINATION "bin"
    ARCHIVE DESTINATION "lib"
    LIBRARY DESTINATION "lib")

install(FILES feeds_client/feeds_cli.conf
    DESTINATION "etc/feeds")
