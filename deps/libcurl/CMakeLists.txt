project(curl)

include(ExternalProject)

if(WIN32)
    set(PATCH_ARGS -s -p1 < "${CMAKE_CURRENT_LIST_DIR}/curl.patch")

    set(CONFIGURE_CMD "echo")
    set(CONFIGURE_ARGS "Done")

    set(BUILD_CMD cd winbuild && nmake)
    set(BUILD_ARGS
        "/f" "Makefile.vc"
        "MODE=static"
        "WITH_DEVEL=${FEEDS_INT_DIST_DIR}"
        "WITH_SSL=static"
        "WITH_ZLIB=static"
        "ENABLE_SSPI=yes"
        "ENABLE_WINSSL=yes"
        "ENABLE_OPENSSL_AUTO_LOAD_CONFIG=no"
        "ENABLE_IDN=no")

    set(OUTPUT_PATH builds\\libcurl)
    set(INSTALL_CMD
        xcopy "${OUTPUT_PATH}\\include\\curl" "${FEEDS_INT_DIST_DIR}\\include\\curl" /e /i /h /y &&
        copy "${OUTPUT_PATH}\\lib\\libcurl_a.lib" "${FEEDS_INT_DIST_DIR}\\lib\\curl.lib" /y)
else()
    include(ExternalConfigureArgs)

    if(DARWIN)
        set(SSL_OPTIONS
            "--with-darwinssl"
            "--with-secure-transport"
            "--enable-proxy")
    else()
        set(SSL_OPTIONS
            "--without-darwinssl"
            "--disable-proxy")
    endif()

    set(PATCH_EXE "echo")
    set(PATCH_ARGS "Done")

    set(CONFIGURE_CMD "./configure")
    set(CONFIGURE_ARGS
        "--prefix=${FEEDS_INT_DIST_DIR}"
        "--with-ssl=${FEEDS_INT_DIST_DIR}"
        "--with-zlib=${FEEDS_INT_DIST_DIR}"
        "--disable-shared"
        "--enable-static"
        "--without-brotli"
        "--without-ldap-lib"
        "--without-lber-lib"
        "--without-winssl"
        "--without-schannel"
        "--without-gnutls"
        "--without-polarssl"
        "--without-mbedtls"
        "--without-cyassl"
        "--without-wolfssl"
        "--without-libpsl"
        "--without-libmetalink"
        "--without-libssh2"
        "--without-libssh"
        "--without-librtmp"
        "--without-winidn"
        "--without-libidn2"
        "--without-nghttp2"
        "--enable-http"
        "--disable-ftp"
        "--disable-file"
        "--disable-ldap"
        "--disable-ldaps"
        "--disable-rtsp"
        "--disable-dict"
        "--disable-telnet"
        "--disable-tftp"
        "--disable-pop3"
        "--disable-imap"
        "--disable-smb"
        "--disable-smtp"
        "--disable-gopher"
        "--disable-manual"
        ${SSL_OPTIONS}
        ${CONFIGURE_ARGS_INIT})

    set(INSTALL_CMD
        make install)
endif()

ExternalProject_Add(
    curl

    PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
    URL "https://github.com/curl/curl/releases/download/curl-7_71_1/curl-7.71.1.tar.bz2"
    URL_HASH SHA256=9d52a4d80554f9b0d460ea2be5d7be99897a1a9f681ffafe739169afd6b4f224
    DOWNLOAD_NAME "curl-7.71.1.tar.bz2"
    DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1
    LOG_DOWNLOAD FALSE

    BUILD_IN_SOURCE 1

    PATCH_COMMAND ${PATCH_EXE} ${PATCH_ARGS}
    CONFIGURE_COMMAND ${CONFIGURE_CMD} ${CONFIGURE_ARGS}
    BUILD_COMMAND ${BUILD_CMD} ${BUILD_ARGS}
    INSTALL_COMMAND ${INSTALL_CMD}
)

set(CURL_DEPS
    curl
    ssl
    crypto
    z)

if(WIN32)
    set(CURL_DEPS ${CURL_DEPS} Crypt32 Ws2_32 Iphlpapi)
endif()

if(NOT ANDROID)
    set(CURL_DEPS ${CURL_DEPS} pthread)
endif()

add_library(libcurl INTERFACE)
target_link_libraries(libcurl INTERFACE ${CURL_DEPS})
if(WIN32)
    target_compile_definitions(libcurl INTERFACE CURL_STATICLIB)
endif()

add_dependencies(libcurl
    libressl)

if(ENABLE_STATIC)
    export_static_library(curl)
endif()
