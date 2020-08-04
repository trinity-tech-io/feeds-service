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
    msgq.c
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
set_target_properties(feeds_daemon PROPERTIES OUTPUT_NAME ela-feedsd)
target_link_libraries(feeds_daemon ${LIBS})

install(TARGETS feeds_daemon
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    COMPONENT core)

install(FILES feedsd.conf
    DESTINATION etc/ela-feedsd
    COMPONENT core)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    install(FILES ${CMAKE_SOURCE_DIR}/scripts/ela-feedsd.service
        DESTINATION lib/systemd/system
        COMPONENT core)

    install(PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/ela-feedsd.sh
        DESTINATION etc/init.d
        RENAME ela-feedsd
        COMPONENT core)
endif()

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

add_executable(feeds_cli EXCLUDE_FROM_ALL ${SRC})
add_dependencies(feeds_cli ${FEEDSCLI_DEPENDS})
set_target_properties(feeds_cli PROPERTIES OUTPUT_NAME ela-feeds-cli)

add_custom_target(utils
    DEPENDS feeds_cli)

target_link_libraries(feeds_cli ${LIBS})
install(TARGETS feeds_cli
    EXCLUDE_FROM_ALL
    COMPONENT utils
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)

install(FILES feeds_client/feeds_cli.conf
    EXCLUDE_FROM_ALL
    COMPONENT utils
    DESTINATION etc/ela-feedsd)

add_custom_target(install_utils
    COMMAND ${CMAKE_COMMAND}
        -DCOMPONENT=utils
        -P ${CMAKE_BINARY_DIR}/cmake_install.cmake
    DEPENDS utils)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(CPACK_GENERATOR DEB)

    set(CPACK_PACKAGE_DESCRIPTION "Elastos Feeds Service Distribution Packages")
    set(CPACK_PACKAGE_VENDOR elastos.org)
    set(CPACK_PACKAGE_CONTACT libin@elastos.org)
    set(CPACK_PACKAGING_INSTALL_PREFIX /)
    set(CPACK_PACKAGE_CHECKSUM MD5)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    if(RASPBERRYPI)
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE armhf)
    else()
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
    endif()

    set(CPACK_COMPONENTS_ALL core)

    set(CPACK_DEBIAN_PACKAGE_PREDEPENDS adduser)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "lsb-base (>= 3.0), init-system-helpers (>= 1.18~),
        libc6 (>= 2.14), libsystemd0")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "http://www.elastos.org/")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
        "Elastos Feeds service daemon.")

    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        ${CMAKE_SOURCE_DIR}/debian/postinst
        ${CMAKE_SOURCE_DIR}/debian/postrm
        ${CMAKE_SOURCE_DIR}/debian/preinst
        ${CMAKE_SOURCE_DIR}/debian/prerm
        ${CMAKE_SOURCE_DIR}/debian/conffiles)

    include(CPack)
endif()
