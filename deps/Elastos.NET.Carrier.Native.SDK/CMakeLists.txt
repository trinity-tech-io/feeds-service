project(Elastos.NET.Carrier.Native.SDK)

include(ExternalProject)

ExternalProject_Add(
    Elastos.NET.Carrier.Native.SDK

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL https://github.com/elastos/Elastos.NET.Carrier.Native.SDK/archive/release-v5.6.1.tar.gz
    URL_HASH SHA256=33230020a71b60f2adf005cadacc11bfc8f131e49ec7576c54192e0a233a8ae3
    DOWNLOAD_NAME Elastos.NET.Carrier.Native.SDK-release-v5.6.1.tar.gz
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DENABLE_SHARED=TRUE
        -DENABLE_STATIC=FALSE
        -DENABLE_APPS=FALSE
        -DENABLE_TESTS=FALSE
        -DENABLE_DOCS=FALSE
        ${EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS}
)

export_shared_library(elacarrier)

add_library(libelacarrier-shared INTERFACE)
target_link_libraries(libelacarrier-shared INTERFACE elacarrier libcrystal-shared)
