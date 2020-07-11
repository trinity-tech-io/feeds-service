project(CUnit)

include(ExternalProject)

set(CONFIGURE_CMD
    aclocal &&
    autoconf &&
    autoreconf -if &&
    ./configure)

set(CONFIGURE_ARGS
    --prefix=${FEEDS_INT_DIST_DIR}
    --enable-shared=no
    --disable-shared
    --enable-static=yes)

set(BUILD_CMD make)

set(INSTALL_CMD make)
set(INSTALL_ARGS install)

ExternalProject_Add(
    CUnit

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL http://jaist.dl.sourceforge.net/project/cunit/CUnit/2.1-3/CUnit-2.1-3.tar.bz2
    URL_HASH SHA256=f5b29137f845bb08b77ec60584fdb728b4e58f1023e6f249a464efa49a40f214
    DOWNLOAD_NAME CUnit-2.1-3.tar.bz2
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1

    BUILD_IN_SOURCE 1

    PATCH_COMMAND ${PATCH_EXE} -s -p1 < ${CMAKE_CURRENT_LIST_DIR}/CUnit.patch

    CONFIGURE_COMMAND ${CONFIGURE_CMD} ${CONFIGURE_ARGS}
    BUILD_COMMAND ${BUILD_CMD} ${BUILD_ARGS}
    INSTALL_COMMAND ${INSTALL_CMD} ${INSTALL_ARGS}
)
