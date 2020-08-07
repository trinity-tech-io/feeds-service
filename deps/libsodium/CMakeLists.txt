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
    URL https://github.com/jedisct1/libsodium/releases/download/1.0.11/libsodium-1.0.11.tar.gz
    URL_HASH SHA256=a14549db3c49f6ae2170cbbf4664bd48ace50681045e8dbea7c8d9fb96f9c765
    DOWNLOAD_NAME libsodium-1.0.11.tar.gz
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1

    BUILD_IN_SOURCE 1

    PATCH_COMMAND ${PATCH_EXE} -s -p1 < ${CMAKE_CURRENT_LIST_DIR}/libsodium.patch

    CONFIGURE_COMMAND ${CONFIGURE_CMD} ${CONFIGURE_ARGS}
    BUILD_COMMAND ${BUILD_CMD} ${BUILD_ARGS}
    INSTALL_COMMAND ${INSTALL_CMD} ${INSTALL_ARGS}
)
