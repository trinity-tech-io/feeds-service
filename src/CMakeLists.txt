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

add_definitions(-DLIBCONFIG_STATIC)

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
    carrier
    did
    msgpack-c
    libcrystal
    libconfig
    libqrencode
    sqlitecpp-static
    cvector
    mkdirs
    sandbird)

if(WIN32)
    set(CONFIG_LIBS libconfig.lib)
    set(SYSTEM_LIBS Winmm Ws2_32 Iphlpapi Shlwapi)
else()
    set(CONFIG_LIBS config)
    set(SYSTEM_LIBS dl m)
endif()

set(LIBS
    libcarrier-static
    libeladid-static
    libqrencode-static
    crystal
    cvector
    mkdirs
    sandbird
    pthread)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN
        -D_CRT_SECURE_NO_WARNINGS
        -D_CRT_NONSTDC_NO_WARNINGS)

    # Force source code encoding to utf-8
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
endif()

target_link_libraries(feedsd
    msgpackc
    sqlite3
    ${LIBS}
    ${CONFIG_LIBS}
    ${SYSTEM_LIBS})

set(FEEDSD_LISTEN_IP 0.0.0.0)
if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(FEEDSD_LOG_DIR /var/log/feedsd)
    set(FEEDSD_DATA_DIR /var/lib/feedsd)
else()
    set(FEEDSD_LOG_DIR ~/.feedsd)
    set(FEEDSD_DATA_DIR ~/.feedsd)
endif()

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

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(CPACK_GENERATOR DEB)

    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
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
