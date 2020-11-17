project(Elastos.DID.Native.SDK)

include(ExternalProject)

ExternalProject_Add(
    Elastos.DID.Native.SDK

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL https://github.com/elastos/Elastos.DID.Native.SDK/archive/v1.2.zip
    URL_HASH SHA256=da7b87c8d6b0e7856c466e24d1ac7a65e9f23f800d3800f887932b35afb6143f
    DOWNLOAD_NAME Elastos.DID.Native.SDK-v1.2.zip
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
