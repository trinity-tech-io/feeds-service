project(libressl)

include(ExternalProject)

ExternalProject_Add(
    libressl

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://github.com/elastos/libressl-portable/archive/v2.9.2.tar.gz"
    URL_HASH SHA256=27888e600cf61395d4129692afe2403854a2324d3c26c609d93490fde40ea253
    DOWNLOAD_NAME "libressl-2.9.2.tar.gz"
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DLIBRESSL_SKIP_INSTALL=OFF
        -DLIBRESSL_APPS=OFF
        -DLIBRESSL_TESTS=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DENABLE_ASM=OFF
        -DENABLE_EXTRATESTS=OFF
        -DENABLE_NC=OFF
        ${CMAKE_ARGS_INIT}
)

if(WIN32)
    ExternalProject_Add_Step(
        libressl post-install
        DEPENDEES install
        WORKING_DIRECTORY "${FEEDS_INT_DIST_DIR}/lib"
        COMMAND copy crypto.lib libeay32.lib /y
        COMMAND copy ssl.lib ssleay32.lib /y
    )
endif()

if(ENABLE_STATIC)
    export_static_library(ssl)
    export_static_library(tls)
    export_static_library(crypto)
    if(WIN32)
        export_static_library(crypt32)
    endif()
endif()
