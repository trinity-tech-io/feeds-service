project(json)

include(ExternalProject)

set(JSON_INSTALL_DIR ${FEEDS_INT_DIST_DIR}/include/nlohmann)

ExternalProject_Add(
	json

	PREFIX ${FEEDS_DEPS_BUILD_PREFIX}
	URL "https://github.com/nlohmann/json/releases/download/v3.9.1/json.hpp"
	URL_HASH SHA256=7804b38146921d03374549c9e2a5e3acda097814c43caf2b96a0278e58df26e0
	DOWNLOAD_NAME json.hpp
	DOWNLOAD_DIR ${FEEDS_DEPS_TARBALL_DIR}
	DOWNLOAD_NO_EXTRACT 1
	DOWNLOAD_NO_PROGRESS 1

	CONFIGURE_COMMAND ${CMAKE_COMMAND} -E make_directory ${JSON_INSTALL_DIR}
	BUILD_COMMAND ""
	INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${FEEDS_DEPS_TARBALL_DIR}/json.hpp ${JSON_INSTALL_DIR}/json.hpp
)
