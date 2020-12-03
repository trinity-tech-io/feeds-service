project(Elastos.DID.Native.SDK)

include(ExternalProject)

ExternalProject_Add(
    Elastos.DID.Native.SDK

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL https://github.com/elastos/Elastos.DID.Native.SDK/archive/14073f6173bc4ebff03ade0cccefd6d9082d60c3.zip
    URL_HASH SHA256=37e74234e8a032831c6ff2ab4e38e668d755f99ddb0f3fb32aae25f8756c20ae
    DOWNLOAD_NAME Elastos.DID.Native.SDK-release-v1.x.zip
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
        -DENABLE_JWT=TRUE
        ${EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS}
)

export_shared_library(eladid)

add_library(libeladid-shared INTERFACE)
target_link_libraries(libeladid-shared INTERFACE eladid libcrystal-shared)
