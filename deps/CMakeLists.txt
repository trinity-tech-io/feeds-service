include(AddSubModule)

add_submodule(libconfig)
add_submodule(msgpack-c)
add_submodule(Elastos.DID.Native.SDK)
add_submodule(Elastos.NET.Carrier.Native.SDK)
add_submodule(libsodium)
add_submodule(libcrystal DEPENDS libsodium)
add_submodule(sqlite)
add_submodule(CUnit)
add_submodule(zlib)
add_submodule(libpng DEPENDS zlib)
add_submodule(libqrencode DEPENDS libpng)


