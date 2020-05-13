project(libpng)

include(FeedsDefaults)
include(ExternalProject)

ExternalProject_Add(
    libpng

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://downloads.sourceforge.net/libpng/libpng-1.6.37.tar.xz"
    URL_HASH SHA256=505e70834d35383537b6491e7ae8641f1a4bed1876dbfe361201fc80868d88ca
    DOWNLOAD_NAME "libpng-1.6.37.tar.xz"
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DPNG_SHARED=OFF
        -DPNG_STATIC=ON
        -DPNG_TESTS=OFF
        "-DCMAKE_PREFIX_PATH=${FEEDS_INT_DIST_DIR}"
)
