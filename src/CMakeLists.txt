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

include_directories(
    .
    ${FEEDS_INT_DIST_DIR}/include)

link_directories(
    ${FEEDS_INT_DIST_DIR}/lib)

add_executable(feedsd
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

add_dependencies(feedsd
    Elastos.NET.Carrier.Native.SDK
    Elastos.DID.Native.SDK
    msgpack-c
    libcrystal
    libconfig
    libqrencode
    sqlite)

target_link_libraries(feedsd
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

install(TARGETS feedsd
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    COMPONENT main)

install(FILES feedsd.conf
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

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(CPACK_GENERATOR DEB)

    set(CPACK_PACKAGE_DESCRIPTION "Feeds Service Distribution Packages")
    set(CPACK_PACKAGE_VENDOR trinity-tech.io)
    set(CPACK_PACKAGE_CONTACT lifayi@trinity-tech.io)
    set(CPACK_PACKAGING_INSTALL_PREFIX /)
    set(CPACK_PACKAGE_CHECKSUM MD5)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    if(RASPBERRYPI)
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE armhf)
    else()
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
    endif()

    set(CPACK_COMPONENTS_ALL main)

    set(CPACK_DEBIAN_PACKAGE_PREDEPENDS adduser)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "lsb-base (>= 3.0), init-system-helpers (>= 1.18~),
        libc6 (>= 2.14), libsystemd0")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "http://www.trinity-tech.io/")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
        "Feeds service daemon.")

    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        ${CMAKE_SOURCE_DIR}/debian/postinst
        ${CMAKE_SOURCE_DIR}/debian/postrm
        ${CMAKE_SOURCE_DIR}/debian/preinst
        ${CMAKE_SOURCE_DIR}/debian/prerm
        ${CMAKE_SOURCE_DIR}/debian/conffiles)

    include(CPack)
endif()
