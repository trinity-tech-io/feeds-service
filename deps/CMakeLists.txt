include(AddSubModule)

add_submodule(platform-specific)
add_submodule(libconfig
    DEPENDS platform-specific)
add_submodule(cJSON
    DEPENDS platform-specific)
add_submodule(Elastos.NET.Carrier.Native.SDK
    DEPENDS platform-specific)
add_submodule(sqlite
    DEPENDS platform-specific)
add_submodule(CUnit
    DEPENDS platform-specific)


