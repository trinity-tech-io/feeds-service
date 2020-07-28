project(libcrystal)

include(FeedsDefaults)
include(ExternalProject)

ExternalProject_Add(
    libcrystal

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://github.com/iwhisperio/libcrystal/archive/release-v1.0.5.tar.gz"
    URL_HASH SHA256=6256713f34bb341d718a38b5f3a108c6d9c1db93e6391f670108de1cd09b3cee
    DOWNLOAD_NAME "libcrystal-v1.0.5.tar.gz"
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1

    PATCH_COMMAND ${PATCH_EXE} -s -p1 < ${CMAKE_CURRENT_LIST_DIR}/libcrystal.patch

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DENABLE_SHARED=TRUE
        -DENABLE_STATIC=FALSE
        -DWITH_LIBSODIUM=${FEEDS_INT_DIST_DIR}
        -DLIBSODIUM_STATIC=TRUE
        -DENABLE_CRYPTO=TRUE
        -DENABLE_TESTS=FALSE
        ${EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS}
)

ExternalProject_Get_property(Elastos.DID.Native.SDK STAMP_DIR)
set(DID_STAMP_DIR ${STAMP_DIR})

ExternalProject_Get_property(Elastos.NET.Carrier.Native.SDK STAMP_DIR)
set(CARRIER_STAMP_DIR ${STAMP_DIR})

ExternalProject_Add_StepDependencies(
    libcrystal
    install
    Elastos.DID.Native.SDK
    ${DID_STAMP_DIR}/Elastos.DID.Native.SDK-done
    Elastos.NET.Carrier.Native.SDK
    ${CARRIER_STAMP_DIR}/Elastos.NET.Carrier.Native.SDK-done
)

export_shared_library(crystal)

add_library(libcrystal-shared INTERFACE)
target_link_libraries(libcrystal-shared INTERFACE crystal)
