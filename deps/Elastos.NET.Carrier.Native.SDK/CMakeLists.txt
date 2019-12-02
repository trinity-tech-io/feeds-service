project(Elastos.NET.Carrier.Native.SDK)

include(FeedsDefaults)
include(ExternalProject)

ExternalProject_Add(
    Elastos.NET.Carrier.Native.SDK

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://github.com/elastos/Elastos.NET.Carrier.Native.SDK/archive/169258864425eeefca422e7cc7e23f92e0147bcc.zip"
    URL_HASH SHA256=fd04f941a51105b14865de74e29ad2e6566d60eade4c487fc98bcfde3568a6d2
    DOWNLOAD_NAME "Elastos.NET.Carrier.Native.SDK-169258864425eeefca422e7cc7e23f92e0147bcc.zip"
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DENABLE_SHARED=TRUE
        -DENABLE_STATIC=FALSE
        -DENABLE_APPS=FALSE
        -DENABLE_TESTS=FALSE
        -DENABLE_DOCS=FALSE
)

ExternalProject_Add_Step(
    Elastos.NET.Carrier.Native.SDK post-install
    DEPENDEES install
    WORKING_DIRECTORY "<BINARY_DIR>/intermediates/include"
    COMMAND ${CMAKE_COMMAND} -E copy crystal.h "${FEEDS_INT_DIST_DIR}/include/crystal.h"
    COMMAND ${CMAKE_COMMAND} -E copy_directory crystal "${FEEDS_INT_DIST_DIR}/include/crystal"
)

export_shared_library(crystal)
export_shared_library(elacarrier)

add_library(libcrystal-shared INTERFACE)
target_link_libraries(libcrystal-shared INTERFACE crystal)

add_library(libelacarrier-shared INTERFACE)
target_link_libraries(libelacarrier-shared INTERFACE elacarrier libcrystal-shared)
