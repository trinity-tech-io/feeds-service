project(Elastos.DID.Native.SDK)

include(FeedsDefaults)
include(ExternalProject)

ExternalProject_Add(
    Elastos.DID.Native.SDK

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://github.com/elastos/Elastos.DID.Native.SDK/archive/2b9894aa8b4b48acd66104446c3edb1483db7c42.zip"
    URL_HASH SHA256=5d0cfbb7b6587eeb7109d0d091bdc5a52ccac21617304c2f18f130cc5db15fb2
    DOWNLOAD_NAME "Elastos.DID.Native.SDK-2b9894aa8b4b48acd66104446c3edb1483db7c42.zip"
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DENABLE_SHARED=TRUE
        -DENABLE_STATIC=TRUE
        -DENABLE_APPS=FALSE
        -DENABLE_TESTS=FALSE
        -DENABLE_DOCS=FALSE
        -DENABLE_SPVADAPTER=FALSE
        -DHDKEY_ONLY=FALSE
)

export_shared_library(eladid)

add_library(libeladid-shared INTERFACE)
target_link_libraries(libeladid-shared INTERFACE eladid libcrystal-shared)
