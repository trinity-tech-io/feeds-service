project(msgpack-c)

include(FeedsDefaults)
include(ExternalProject)

ExternalProject_Add(
    msgpack-c

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://github.com/msgpack/msgpack-c/archive/cpp-3.2.1.tar.gz"
    URL_HASH SHA256=464f46744a6be778626d11452c4db3c2d09461080c6db42e358e21af19d542f6
    DOWNLOAD_NAME "msgpack-c-cpp-3.2.1.tar.gz"
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DBUILD_SHARED_LIBS=OFF
)
