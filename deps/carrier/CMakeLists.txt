project(carrier)

include(ExternalProject)

ExternalProject_Add(
    carrier

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL https://github.com/elastos/Elastos.NET.Carrier.Native.SDK/archive/release-v6.0.3.tar.gz
    URL_HASH SHA256=daa79d1bae6d52ce4efb3753eb4b7892441a2d51ebd5baa36a38416550346e65
    DOWNLOAD_NAME carrier-v6.0.3.tar.gz
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DENABLE_SHARED=FALSE
        -DENABLE_STATIC=TRUE
        -DENABLE_APPS=FALSE
        -DENABLE_TESTS=FALSE
        -DENABLE_DOCS=FALSE
        -DCMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}
        -DCMAKE_VS_PLATFORM_TOOLSET=${CMAKE_VS_PLATFORM_TOOLSET}
        ${EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS}
)

if(WIN32)
    set(LIB_DECORATOR "lib")
else()
    set(LIB_DECORATOR "")
endif()

add_library(libcarrier-static INTERFACE)
target_link_libraries(libcarrier-static INTERFACE carriersession carrier
                                                  pjmedia pjnath pj${LIB_DECORATOR} pjlib-util
                                                  toxcore ${LIB_DECORATOR}srtp flatccrt
                                                  curl ssl tls crypto z
                                                  libcrystal-static)
target_include_directories(libcarrier-static INTERFACE ${FEEDS_INT_DIST_DIR}/include)
