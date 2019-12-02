if(__feeds_defaults_included)
    return()
endif()
set(__feeds_defaults_included TRUE)

# Third-party dependency tarballs directory
set(FEEDS_DEPS_TARBALL_DIR "${CMAKE_SOURCE_DIR}/build/.tarballs")
set(FEEDS_DEPS_BUILD_PREFIX "external")

# Intermediate distribution directory
set(FEEDS_INT_DIST_DIR "${CMAKE_BINARY_DIR}/intermediates")
if(WIN32)
    file(TO_NATIVE_PATH
        "${FEEDS_INT_DIST_DIR}" FEEDS_INT_DIST_DIR)
endif()

# Host tools directory
set(FEEDS_HOST_TOOLS_DIR "${CMAKE_BINARY_DIR}/host")
if(WIN32)
    file(TO_NATIVE_PATH
         "${FEEDS_HOST_TOOLS_DIR}" FEEDS_HOST_TOOLS_DIR)
endif()

if(WIN32)
    set(PATCH_EXE "${FEEDS_HOST_TOOLS_DIR}/usr/bin/patch.exe")
else()
    set(PATCH_EXE "patch")
endif()

set(CMAKE_MACOSX_RPATH TRUE)
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_RPATH "${FEEDS_INT_DIST_DIR}/lib")
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

function(export_static_library MODULE_NAME)
    set(_INSTALL_DESTINATION lib)

    string(CONCAT STATIC_LIBRARY_NAME
        "${FEEDS_INT_DIST_DIR}/${_INSTALL_DESTINATION}/"
        "${CMAKE_STATIC_LIBRARY_PREFIX}"
        "${MODULE_NAME}"
        "${CMAKE_STATIC_LIBRARY_SUFFIX}")

    file(RELATIVE_PATH STATIC_LIBRARY_NAME ${CMAKE_CURRENT_LIST_DIR}
        ${STATIC_LIBRARY_NAME})

    install(FILES "${STATIC_LIBRARY_NAME}"
        DESTINATION ${_INSTALL_DESTINATION})
endfunction()

function(export_shared_library MODULE_NAME)
    if(WIN32)
        set(_INSTALL_DESTINATION bin)
    else()
        set(_INSTALL_DESTINATION lib)
    endif()

    string(CONCAT SHARED_LIBRARY_NAME
        "${FEEDS_INT_DIST_DIR}/${_INSTALL_DESTINATION}/"
        "${CMAKE_SHARED_LIBRARY_PREFIX}"
        "${MODULE_NAME}"
        "${CMAKE_SHARED_LIBRARY_SUFFIX}")

    file(RELATIVE_PATH SHARED_LIBRARY_NAME ${CMAKE_CURRENT_LIST_DIR}
        ${SHARED_LIBRARY_NAME})

    install(PROGRAMS "${SHARED_LIBRARY_NAME}"
        DESTINATION ${_INSTALL_DESTINATION})
endfunction()
