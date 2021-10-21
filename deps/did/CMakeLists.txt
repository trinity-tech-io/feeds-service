project(did)

include(ExternalProject)

ExternalProject_Add(
    did

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL https://github.com/elastos/Elastos.DID.Native.SDK/archive/refs/tags/release-v2.1.3.tar.gz
    URL_HASH SHA256=7ad7d06b833b5afb2ac4b3b31fe89c97805233e4c1d28fef94e77b59a2d29c80
    DOWNLOAD_NAME did-v2.1.3.tar.gz
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DENABLE_SHARED=FALSE
        -DENABLE_STATIC=TRUE
        -DENABLE_APPS=FALSE
        -DENABLE_TESTS=FALSE
        -DENABLE_DOCS=FALSE
        -DHDKEY_ONLY=FALSE
        -DENABLE_JWT=TRUE
        ${EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS}
)

add_library(libeladid-static INTERFACE)

if(WIN32)
    set(LIB_DECORATOR "_s")
else()
    set(LIB_DECORATOR "")
endif()

target_link_libraries(libeladid-static INTERFACE eladid${LIB_DECORATOR} hdkey${LIB_DECORATOR}
                                                 cjose jansson zip)

