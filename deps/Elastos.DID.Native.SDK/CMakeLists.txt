project(Elastos.DID.Native.SDK)

include(FeedsDefaults)
include(ExternalProject)

ExternalProject_Add(
    Elastos.DID.Native.SDK

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://github.com/elastos/Elastos.DID.Native.SDK/archive/c05bd04ae51c7543ea1425d0b080e61f7c7997aa.zip"
    URL_HASH SHA256=b6dc861fbf41c933026d7d8a3877aeb688fb81ce7bffe5f91636fc311314af1a
    DOWNLOAD_NAME "Elastos.DID.Native.SDK-c05bd04ae51c7543ea1425d0b080e61f7c7997aa.zip"
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

_ep_get_complete_stampfile(Elastos.DID.Native.SDK DID_STAMPFILE)
set(ENV{DID_STAMPFILE} "${DID_STAMPFILE}")

export_shared_library(eladid)

add_library(libeladid-shared INTERFACE)
target_link_libraries(libeladid-shared INTERFACE eladid libcrystal-shared)
