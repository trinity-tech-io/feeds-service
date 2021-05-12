project(pthreads-w32)

include(FeedsDefaults)
include(ExternalProject)
include(ExternalCMakeArgs)

set(CP ${CMAKE_COMMAND} -E copy)

ExternalProject_Add(
    pthreads-w32

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-9-1-release.zip"
    URL_HASH SHA256=b9bd02958639a854461a72f72bf3e89a3c4181843b65924a54df8a50b4e5d15d
    DOWNLOAD_NAME "pthreads-w32-2-9-1-release.zip"
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1

    BINARY_DIR "${FEEDS_DEPS_BUILD_PREFIX}/src/pthreads-w32/pthreads.2"

    PATCH_COMMAND ${PATCH_EXE} -s -p1 < ${CMAKE_CURRENT_LIST_DIR}/pthreads-w32.patch

    CONFIGURE_COMMAND echo Done.
    BUILD_COMMAND nmake VC DEVROOT=${FEEDS_INT_DIST_DIR}
    INSTALL_COMMAND nmake install DEVROOT=${FEEDS_INT_DIST_DIR}
            COMMAND ${CP} ${FEEDS_INT_DIST_DIR}\\lib\\pthreadVC2.lib ${FEEDS_INT_DIST_DIR}\\lib\\pthread.lib
)

set(SHARED_LIBRARY_NAME
    "${FEEDS_INT_DIST_DIR}\\bin\\pthreadVC2.dll")

string(REPLACE "\\" "/" LIBRARY_NAME ${SHARED_LIBRARY_NAME})

file(RELATIVE_PATH LIBRARY_NAME ${CMAKE_CURRENT_LIST_DIR}
    ${LIBRARY_NAME})

install(PROGRAMS ${LIBRARY_NAME}
    DESTINATION bin)
