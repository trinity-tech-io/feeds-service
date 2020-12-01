project(SQLiteCpp)

include(ExternalProject)

ExternalProject_Add(
    SQLiteCpp

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://github.com/SRombauts/SQLiteCpp/archive/3.1.1.tar.gz"
    URL_HASH SHA256=b00b1efad985dd415c3b62b01252cae20d9f2c32dcac435ddd8a105e9d4ddcde
    DOWNLOAD_NAME "SQLiteCpp-3.1.1.tar.gz"
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${FEEDS_INT_DIST_DIR}
        ${EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS}
        -DENABLE_STATIC=TRUE
)

export_static_library(SQLiteCpp)

add_library(sqlitecpp-static INTERFACE)
target_link_libraries(sqlitecpp-static INTERFACE SQLiteCpp)
