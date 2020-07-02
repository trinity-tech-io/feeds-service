project(Elastos.DID.Native.SDK)

include(FeedsDefaults)
include(ExternalProject)

ExternalProject_Add(
    Elastos.DID.Native.SDK

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://github.com/elastos/Elastos.DID.Native.SDK/archive/5719ecfdd5440f9485009492a99206287103fa4f.zip"
    URL_HASH SHA256=e07bd6379187a8ccf4a269ea7fb5db5d025000a0c692bc49406b7b6ea890e913
    DOWNLOAD_NAME "Elastos.DID.Native.SDK-5719ecfdd5440f9485009492a99206287103fa4f.zip"
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
