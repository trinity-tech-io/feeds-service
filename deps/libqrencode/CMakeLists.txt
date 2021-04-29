project(libqrencode)

include(ExternalProject)

ExternalProject_Add(
    libqrencode

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL https://fukuchi.org/works/qrencode/qrencode-4.0.2.tar.gz
    URL_HASH SHA256=dbabe79c07614625d1f74d8c0ae2ee5358c4e27eab8fd8fe31f9365f821a3b1d
    DOWNLOAD_NAME qrencode-4.0.2.tar.gz
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        -DWITH_TOOLS=NO
        -DCMAKE_PREFIX_PATH=${FEEDS_INT_DIST_DIR}
        ${EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS}
)

add_library(libqrencode-static INTERFACE)

if(WIN32)
    target_link_libraries(libqrencode-static INTERFACE qrencoded libpng16_staticd z)
else()
    target_link_libraries(libqrencode-static INTERFACE qrencode png z)
endif()
