include(AddSubModule)

add_submodule(platform-specific)
add_submodule(libconfig
    DEPENDS platform-specific)
add_submodule(msgpack-c
    DEPENDS platform-specific)
add_submodule(Elastos.DID.Native.SDK
    DEPENDS platform-specific)
add_submodule(Elastos.NET.Carrier.Native.SDK
    DEPENDS platform-specific)
add_submodule(libsodium
    DEPENDS platform-specific)
add_submodule(libcrystal
    DEPENDS platform-specific libsodium)
add_submodule(sqlite
    DEPENDS platform-specific)
add_submodule(CUnit
    DEPENDS platform-specific)
add_submodule(zlib
    DEPENDS platform-specific)
add_submodule(libpng
    DEPENDS zlib platform-specific)
add_submodule(libqrencode
    DEPENDS libpng platform-specific)


