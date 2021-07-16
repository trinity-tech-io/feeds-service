project(did)

include(ExternalProject)

ExternalProject_Add(
    did

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL https://github.com/elastos/Elastos.DID.Native.SDK/archive/refs/tags/release-v2.1.1.tar.gz
    URL_HASH SHA256=ed7fce1aa95d2086329658745197233593e481b63c8c7298b6928bcc3c7b9342
    DOWNLOAD_NAME did-v2.1.1.zip
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

