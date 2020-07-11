# Configurable variables.
# Download raspberrypi toolchain from github: https://github.com/raspberrypi/tools
#
# RPI_TOOLCHAIN_HOME RaspberryPi toolchain directory

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(RASPBERRYPI TRUE)

# The top direcotry of raspberrypi toolchain
set(RPI_TOOLCHAIN_TOPDIR
	${RPI_TOOLCHAIN_HOME}/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64)

set(RPI_HOST arm-linux-gnueabihf)

set(RPI_TOOLCHAIN_PREFIX
	${RPI_TOOLCHAIN_TOPDIR}/bin/${RPI_HOST}-)

# The cross compiler
set(CMAKE_C_COMPILER ${RPI_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${RPI_TOOLCHAIN_PREFIX}g++)
set(CMAKE_CPP_COMPILER "${RPI_TOOLCHAIN_PREFIX}cpp -E")
set(CMAKE_AR ${RPI_TOOLCHAIN_PREFIX}ar CACHE PATH "")
set(CMAKE_RANLIB ${RPI_TOOLCHAIN_PREFIX}ranlib CACHE PATH "")

# CMAKE_SYSROOT
execute_process(
	COMMAND ${CMAKE_C_COMPILER} --print-sysroot
	OUTPUT_VARIABLE CMAKE_SYSROOT
	OUTPUT_STRIP_TRAILING_WHITESPACE)

# CMAKE_<lang>_FLAGS_INIT
string(CONCAT _CMAKE_C_FLAGS_INIT
	"-Ofast "
	"-mthumb "
	"-marm "
	"-march=armv7-a "
	"-mfloat-abi=hard "
	"-mfpu=vfpv3-d16 "
	"-D__arm__ "
	"-Wall "
	"--sysroot=${CMAKE_SYSROOT} ")
set(CMAKE_C_FLAGS_INIT ${_CMAKE_C_FLAGS_INIT})
set(CMAKE_CXX_FLAGS_INIT ${_CMAKE_C_FLAGS_INIT})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
