project(Elastos.NET.Carrier.Native.SDK)

include(ExternalProject)

ExternalProject_Add(
    Elastos.NET.Carrier.Native.SDK

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL https://github.com/elastos/Elastos.NET.Carrier.Native.SDK/archive/release-v5.6.3.tar.gz
    URL_HASH SHA256=6ad79b6e78c56018b48910b98211010878cbfe40a1f7088ef239441bb78146bb
    DOWNLOAD_NAME Elastos.NET.Carrier.Native.SDK-release-v5.6.3.tar.gz
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    PATCH_COMMAND ${PATCH_EXE} -s -p1 < ${CMAKE_CURRENT_LIST_DIR}/carrier.patch

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
target_include_directories(libelacarrier-shared INTERFACE ${FEEDS_INT_DIST_DIR}/include)
