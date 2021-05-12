if(__feeds_defaults_included)
    return()
endif()
set(__feeds_defaults_included TRUE)

# Global default variables defintions
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

if(WIN32)
    set(CMAKE_CPP_FLAGS
        "${CMAKE_CPP_FLAGS} -D_CRT_SECURE_NO_WARNINGS -D_CRT_DEPRECATED_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_CRT_DEPRECATED_NO_WARNINGS)
    add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)
endif()

# Third-party dependency tarballs directory
set(FEEDS_DEPS_TARBALL_DIR ${CMAKE_SOURCE_DIR}/build/.tarballs)
set(FEEDS_DEPS_BUILD_PREFIX external)

# Intermediate distribution directory
set(FEEDS_INT_DIST_DIR ${CMAKE_BINARY_DIR}/intermediates)
if(WIN32)
    file(TO_NATIVE_PATH
        "${FEEDS_INT_DIST_DIR}" FEEDS_INT_DIST_DIR)
endif()

# Host tools directory
set(FEEDS_HOST_TOOLS_DIR ${CMAKE_BINARY_DIR}/host)
if(WIN32)
    file(TO_NATIVE_PATH
        "${FEEDS_HOST_TOOLS_DIR}" FEEDS_HOST_TOOLS_DIR)
endif()

#set(PATCH_EXE patch)

set(CMAKE_MACOSX_RPATH TRUE)
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_RPATH ${FEEDS_INT_DIST_DIR}/lib)
set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib ../lib)

set(EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})

if(RASPBERRYPI)
    get_filename_component(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_TOOLCHAIN_FILE}"
        ABSOLUTE
        BASE_DIR ${CMAKE_BINARY_DIR})

    set(EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS
        ${EXTERNAL_CMAKE_PROJECT_ADDITIONAL_ARGS}
        -DRPI_TOOLCHAIN_HOME=${RPI_TOOLCHAIN_HOME}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DCMAKE_FIND_ROOT_PATH=${FEEDS_INT_DIST_DIR})

    set(EXTERNAL_AUTOCONF_PROJECT_ADDITIONAL_ARGS
        --host=${RPI_HOST}
        CC=${CMAKE_C_COMPILER}
        CXX=${CMAKE_CXX_COMPILER}
        CPP=${CMAKE_CPP_COMPILER}
        AR=${CMAKE_AR}
        RANLIB=${CMAKE_RANLIB}
        CPPFLAGS=--sysroot=${CMAKE_SYSROOT}
        CFLAGS=${CMAKE_C_FLAGS_INIT}
        CXXFLAGS=${CMAKE_CXX_FLAGS_INIT}
        LDFLAGS=--sysroot=${CMAKE_SYSROOT})
endif()

# Host tools directory
if(WIN32)
    set(PATCH_EXE "${FEEDS_HOST_TOOLS_DIR}/usr/bin/patch.exe")
else()
    set(PATCH_EXE "patch")
endif()

if(WIN32)
    set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN")

    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN")
endif()

##Only suport for windows.
if(WIN32)
    function(set_win_build_options build_options suffix)
        # check vs platform.
        # Notice: use CMAKE_SIZEOF_VOID_P to check whether target is
        # 64bit or 32bit instead of using CMAKE_SYSTEM_PROCESSOR.
        if(${CMAKE_SIZEOF_VOID_P} STREQUAL "8")
            set(_PLATFORM "x64")
        else()
            set(_PLATFORM "Win32")
        endif()

        if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
            set(_CONFIGURATION "Debug${suffix}")
        else()
            set(_CONFIGURATION "Release${suffix}")
        endif()

        string(CONCAT _BUILD_OPTIONS
            "/p:"
            "Configuration=${_CONFIGURATION},"
            "Platform=${_PLATFORM},"
            "PlatformToolset=${CMAKE_VS_PLATFORM_TOOLSET},"
            "WindowsTargetPlatformVersion=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION},"
            "InstallDir=${FEEDS_INT_DIST_DIR}")

        # update parent scope variable.
        set(${build_options} "${_BUILD_OPTIONS}" PARENT_SCOPE)
    endfunction()
endif()

function(export_static_library MODULE_NAME)
    set(_INSTALL_DESTINATION lib)

    string(CONCAT STATIC_LIBRARY_NAME
        ${FEEDS_INT_DIST_DIR}/${_INSTALL_DESTINATION}/
        ${CMAKE_STATIC_LIBRARY_PREFIX}
        ${MODULE_NAME}
        ${CMAKE_STATIC_LIBRARY_SUFFIX})

    file(RELATIVE_PATH STATIC_LIBRARY_NAME ${CMAKE_CURRENT_LIST_DIR}
        ${STATIC_LIBRARY_NAME})

    install(FILES ${STATIC_LIBRARY_NAME}
        DESTINATION ${_INSTALL_DESTINATION})
endfunction()

function(export_shared_library MODULE_NAME)
        if(WIN32)
        set(_INSTALL_DESTINATION bin)
    else()
        set(_INSTALL_DESTINATION lib)
    endif()

    string(CONCAT SHARED_LIBRARY_PATTERN
        ${FEEDS_INT_DIST_DIR}/${_INSTALL_DESTINATION}/
        ${CMAKE_SHARED_LIBRARY_PREFIX}
        ${MODULE_NAME}*
        ${CMAKE_SHARED_LIBRARY_SUFFIX}*)

    install(CODE
        "file(GLOB SHARED_LIBRARY_FILES \"${SHARED_LIBRARY_PATTERN}\")
         file(INSTALL \${SHARED_LIBRARY_FILES}
              DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${_INSTALL_DESTINATION}\")")

endfunction()
