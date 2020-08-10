project(libsodium)

include(ExternalProject)

set(CONFIGURE_CMD "./configure")
set(CONFIGURE_ARGS
    --prefix=${FEEDS_INT_DIST_DIR}
    --enable-shared=no
    --disable-shared
    --enable-static
    --with-pic=yes
    --with-pthreads=yes
    ${EXTERNAL_AUTOCONF_PROJECT_ADDITIONAL_ARGS})

if(RASPBERRYPI)
    set(CONFIGURE_ARGS
        ${CONFIGURE_ARGS}
        --with-sysroot=${CMAKE_SYSROOT})
endif()

set(BUILD_CMD make)

set(INSTALL_CMD make)
set(INSTALL_ARGS install)

ExternalProject_Add(
    libsodium

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://github.com/jedisct1/libsodium/releases/download/1.0.18-RELEASE/libsodium-1.0.18.tar.gz"
    URL_HASH SHA256=6f504490b342a4f8a4c4a02fc9b866cbef8622d5df4e5452b46be121e46636c1
    DOWNLOAD_NAME "libsodium-1.0.18.tar.gz"
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1

    BUILD_IN_SOURCE 1

    CONFIGURE_COMMAND ${CONFIGURE_CMD} ${CONFIGURE_ARGS}
    BUILD_COMMAND ${BUILD_CMD} ${BUILD_ARGS}
    INSTALL_COMMAND ${INSTALL_CMD} ${INSTALL_ARGS}
)
