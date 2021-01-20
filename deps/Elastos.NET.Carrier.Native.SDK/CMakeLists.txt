project(Elastos.NET.Carrier.Native.SDK)

include(ExternalProject)

ExternalProject_Add(
    Elastos.NET.Carrier.Native.SDK

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL https://github.com/elastos/Elastos.NET.Carrier.Native.SDK/archive/release-v6.0.2.tar.gz
    URL_HASH SHA256=badfe2bd51ce13ee2192b93a42c0c19396d700f452703fdc9c2a80ffe3279dc5
    DOWNLOAD_NAME Elastos.NET.Carrier.Native.SDK-release-v6.0.2.tar.gz
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DENABLE_SHARED=FALSE
        -DENABLE_STATIC=TRUE
        -DENABLE_APPS=FALSE
        -DENABLE_TESTS=FALSE
        -DENABLE_DOCS=FALSE
        ${EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS}
)

export_shared_library(carrier)
export_shared_library(carriersession)

add_library(libcarrier-shared INTERFACE)
target_link_libraries(libcarrier-shared INTERFACE carrier carriersession
                                                  pj pjlib-util pjmedia pjnath 
                                                  toxcore srtp flatccrt
                                                  curl crypto ssl tls z
                                                  libcrystal-static)
target_include_directories(libcarrier-shared INTERFACE ${FEEDS_INT_DIST_DIR}/include)
