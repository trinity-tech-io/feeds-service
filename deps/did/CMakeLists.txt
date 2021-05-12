project(did)

include(ExternalProject)

ExternalProject_Add(
    did

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL https://github.com/elastos/Elastos.DID.Native.SDK/archive/v1.2-temp.tar.gz
    URL_HASH SHA256=8f88ef7e3f78a3915d7b0f0a0795c6a7e89b32efc28cb2013f6ec078f25b59ad
    DOWNLOAD_NAME did-v1.2-temp.zip
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    PATCH_COMMAND ${PATCH_EXE} -s -p1 < ${CMAKE_CURRENT_LIST_DIR}/did.patch

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DENABLE_SHARED=FALSE
        -DENABLE_STATIC=TRUE
        -DENABLE_APPS=FALSE
        -DENABLE_TESTS=FALSE
        -DENABLE_DOCS=FALSE
        -DENABLE_SPVADAPTER=FALSE
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

target_link_libraries(libeladid-static INTERFACE eladid hdkey${LIB_DECORATOR}
                                                 cjose jansson zip)

