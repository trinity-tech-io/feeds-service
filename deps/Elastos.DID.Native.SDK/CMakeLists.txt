project(Elastos.DID.Native.SDK)

include(ExternalProject)

ExternalProject_Add(
    Elastos.DID.Native.SDK

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL https://github.com/elastos/Elastos.DID.Native.SDK/archive/3010563a3fc11c925852416499c6f73cc5da3ce4.zip
    URL_HASH SHA256=96b429a7059dfaa48747e90bd7bff6dfeeb8b03ec585c424e5cdd207844a35fe
    DOWNLOAD_NAME Elastos.DID.Native.SDK-3010563a3fc11c925852416499c6f73cc5da3ce4.zip
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    PATCH_COMMAND ${PATCH_EXE} -s -p1 < ${CMAKE_CURRENT_LIST_DIR}/did.patch

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DENABLE_SHARED=TRUE
        -DENABLE_STATIC=TRUE
        -DENABLE_APPS=FALSE
        -DENABLE_TESTS=FALSE
        -DENABLE_DOCS=FALSE
        -DENABLE_SPVADAPTER=FALSE
        -DHDKEY_ONLY=FALSE
        ${EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS}
)

export_shared_library(eladid)

add_library(libeladid-shared INTERFACE)
target_link_libraries(libeladid-shared INTERFACE eladid libcrystal-shared)
