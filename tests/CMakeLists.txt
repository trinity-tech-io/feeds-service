include(FeedsDefaults)
include(CheckIncludeFile)
include(CheckFunctionExists)

check_include_file(io.h HAVE_IO_H)
if(HAVE_IO_H)
    add_definitions(-DHAVE_IO_H=1)
endif()
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
check_include_file(sys/resource.h HAVE_SYS_RESOURCE_H)
if(HAVE_SYS_RESOURCE_H)
    add_definitions(-DHAVE_SYS_RESOURCE_H=1)
endif()
check_include_file(direct.h HAVE_DIRECT_H)
if(HAVE_DIRECT_H)
    add_definitions(-DHAVE_DIRECT_H=1)
endif()

check_include_file(process.h HAVE_PROCESS_H)
if(HAVE_PROCESS_H)
    add_definitions(-DHAVE_PROCESS_H=1)
endif()

check_include_file(winsock2.h HAVE_WINSOCK2_H)
if(HAVE_WINSOCK2_H)
    add_definitions(-DHAVE_WINSOCK2_H=1)
endif()

check_include_file(sys/time.h HAVE_SYS_TIME_H)
if(HAVE_SYS_TIME_H)
    add_definitions(-DHAVE_SYS_TIME_H=1)
endif()

check_function_exists(sigaction HAVE_SIGACTION)
if(HAVE_SIGACTION_H)
    add_definitions(-DHAVE_SIGACTION=1)
endif()

set(SRC
    )

add_definitions(-DLIBCONFIG_STATIC)
add_definitions(-DCRYSTAL_DYNAMIC)

set(DEPS
    Elastos.NET.Carrier.Native.SDK
    libconfig
    CUnit
    cJSON)

set(LIBS
    libelacarrier-shared
    libcrystal-shared
    cjson
    cunit
    config
    ncurses
    pthread)

include_directories(
    .
    ../src
    ${FEEDS_INT_DIST_DIR}/include)

link_directories(
    ${FEEDS_INT_DIST_DIR}/lib)

add_executable(feedstests
    main.c
    launcher.c
    test.c
    robot.c
    config.c
    suites/feeds_suite/feeds_suite.c
    ../src/feeds_client/feeds_client.c
    ../src/jsonrpc.c
        ../src/carrier_cfg.c
    ../src/mkdirs.c)

add_dependencies(feedstests ${DEPS})
target_link_libraries(feedstests ${LIBS})

install(TARGETS feedstests
    RUNTIME DESTINATION "bin"
    ARCHIVE DESTINATION "lib"
    LIBRARY DESTINATION "lib")

install(FILES tests.conf
    DESTINATION "etc/feeds")
