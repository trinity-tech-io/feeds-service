include(AddSubModule)

add_submodule(platform-specific)
add_submodule(libconfig
    DEPENDS platform-specific)
add_submodule(msgpack-c)
add_submodule(did)
add_submodule(carrier
    DEPENDS platform-specific)
add_submodule(libsodium
    DEPENDS platform-specific)
add_submodule(libcrystal
    DEPENDS platform-specific libsodium)

if(WIN32)
    add_submodule(pthreads-w32
        DEPENDS platform-specific)
    add_dependencies(libcrystal pthreads-w32)
endif()

add_submodule(zlib)
add_submodule(libpng
    DEPENDS zlib)
add_submodule(libqrencode
    DEPENDS libpng)
add_submodule(c-vector)
add_submodule(mkdirs
    DEPENDS platform-specific libcrystal)
add_submodule(sandbird)
add_submodule(SQLiteCpp)
add_submodule(libressl)
add_submodule(libcurl
    DEPENDS platform-specific)

