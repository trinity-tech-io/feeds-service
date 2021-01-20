project(libcrystal)

include(ExternalProject)

ExternalProject_Add(
    libcrystal

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://github.com/iwhisperio/libcrystal/archive/release-v1.0.6.tar.gz"
    URL_HASH SHA256=24513b4f8971a35a4fd29f85f21dba2ae55e06fe5fe34a533a3013da98a2c98b
    DOWNLOAD_NAME "libcrystal-v1.0.6.tar.gz"
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DENABLE_SHARED=FALSE
        -DENABLE_STATIC=TRUE
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

add_library(libcrystal-static INTERFACE)
target_link_libraries(libcrystal-static INTERFACE crystal sodium)
