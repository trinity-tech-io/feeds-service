project(carrier)

include(ExternalProject)

ExternalProject_Add(
    carrier

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL https://github.com/elastos/Elastos.NET.Carrier.Native.SDK/archive/79c8d02077f25eebf6ce42ba03d0744395c20a29.zip
    URL_HASH SHA256=eebd6ada72b0687250898388ddd7e0e517098a0635b0a77cc12d8600fcc8426e
    DOWNLOAD_NAME carrier-79c8d02.zip
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
