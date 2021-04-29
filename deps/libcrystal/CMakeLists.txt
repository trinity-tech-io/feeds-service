project(libcrystal)

include(ExternalProject)

ExternalProject_Add(
    libcrystal

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://github.com/iwhisperio/libcrystal/archive/release-v1.0.7.tar.gz"
    URL_HASH SHA256=c0f04f0642d92d76839bb4f11bdd24f0b9a7f962b7b8d1ae06c9a06a484f412f
    DOWNLOAD_NAME "libcrystal-v1.0.7.tar.gz"
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DENABLE_SHARED=FALSE
        -DENABLE_STATIC=TRUE
        -DWITH_LIBSODIUM=${FEEDS_INT_DIST_DIR}
        -DWITH_PTHREAD=${FEEDS_INT_DIST_DIR}
        -DLIBSODIUM_STATIC=TRUE
        -DENABLE_CRYPTO=TRUE
        -DENABLE_TESTS=FALSE
        ${EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS}
)

ExternalProject_Get_property(did STAMP_DIR)
set(DID_STAMP_DIR ${STAMP_DIR})

ExternalProject_Get_property(carrier STAMP_DIR)
set(CARRIER_STAMP_DIR ${STAMP_DIR})

ExternalProject_Add_StepDependencies(
    libcrystal
    install
    did
    ${DID_STAMP_DIR}/did-done
    carrier
    ${CARRIER_STAMP_DIR}/carrier-done
)

if(WIN32)
    set(LIB_DECORATOR "")
else()
    set(LIB_DECORATOR "-static")
endif()

add_library(libcrystal-static INTERFACE)
target_link_libraries(libcrystal-static INTERFACE crystal libsodium${LIB_DECORATOR})
