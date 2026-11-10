# Cross toolchain: build the Windows (static) tone-player-qt with MinGW-w64 on Linux.
#
# Requires the Arch packages from the `ownstuff` repository:
#   mingw-w64-qt6-base-static 6.12.0-2   (CMake configs live in
#   mingw-w64-qt6-multimedia-static     /usr/x86_64-w64-mingw32/static)
# plus the shared mingw-w64-qt6-base for the Windows headers of the host.
#
# Usage:
#   cmake --preset windows
#   cmake --build --preset windows
#
# The produced TonePlayer.exe is statically linked and self-contained.
# Qt host tools (moc/rcc/uic) are taken from the Linux Qt 6 of the same version.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Target (Windows) Qt: static libraries and CMake package configs.
set(CMAKE_PREFIX_PATH
    /usr/x86_64-w64-mingw32/static
    /usr/x86_64-w64-mingw32
)
set(CMAKE_FIND_ROOT_PATH
    /usr/x86_64-w64-mingw32/static
    /usr/x86_64-w64-mingw32
)

# Host (Linux) Qt 6.12 — generator tools.
set(QT_HOST_PATH /usr CACHE PATH "Host Qt 6 installation (moc/rcc/uic)")

# --- Static link against the MinGW dependencies ------------------------------
# The static Qt 6 build reports dynamic ("system") wrap dependencies, which
# makes TonePlayer.exe depend on the matching MinGW runtime DLLs (libzstd,
# libfreetype, ...). Force the static archives instead: CMake's find_library
# honors pre-seeded cache variables, and the harfbuzz config only exports
# SHARED targets, so its discovery must be disabled for the static fallback.
set(CMAKE_DISABLE_FIND_PACKAGE_harfbuzz TRUE)
# …and also send pkg-config lookups to an empty directory, so its
# libharfbuzz/libzstd/libpcre2 hits cannot shadow the pre-seeded static
# archives below. With no .pc files found, the find_library fallbacks use
# the pre-seeded *_LIBRARIES paths.
set(PKG_CONFIG_PATH "" CACHE PATH "unused marker" FORCE)
set(PKG_CONFIG_LIBDIR "/nonexistent-pc-dir" CACHE PATH "unused marker" FORCE)
set(PKG_CONFIG_SYSROOT_DIR "" CACHE PATH "unused marker" FORCE)
#
# FindPkgConfig binds the program path in the cache on first lookup, so
# finding "nothing" reliably means pointing it at a wrapper that hides all
# .pc files. With no .pc results, the find_library fallbacks use the
# pre-seeded *_LIBRARIES archives.
# Make Qt's FindWrapZSTD prefer zstd::libzstd_static (its config ships both,
# shared is otherwise picked, leaking libzstd.dll into the import table).
set(CMAKE_FIND_LIBRARY_SUFFIXES_OVERRIDE TRUE CACHE BOOL
    "prefer static zstd in Qt's wrap" FORCE)

set(PKG_CONFIG_EXECUTABLE "${CMAKE_CURRENT_LIST_DIR}/pkg-config-static.sh" CACHE FILEPATH
    "pkg-config wrapper that hides .pc files for the static link" FORCE)
# Qt's FindWrapBrotli consults unofficial-brotli (vcpkg) first; the MinGW
# sysroot has no such config, so point it at the bundled compat package.
set(unofficial-brotli_DIR "${CMAKE_CURRENT_LIST_DIR}/compat/unofficial-brotli" CACHE PATH
    "compat config: static brotli for Qt" FORCE)
set(HARFBUZZ_INCLUDE_DIRS "/usr/x86_64-w64-mingw32/include/harfbuzz" CACHE PATH
    "harfbuzz include path for the static fallback" FORCE)
set(HARFBUZZ_LIBRARIES "/usr/x86_64-w64-mingw32/lib/libharfbuzz.a" CACHE FILEPATH
    "static harfbuzz for the Qt wrap" FORCE)

set(FREETYPE_LIBRARY "/usr/x86_64-w64-mingw32/lib/libfreetype.a" CACHE FILEPATH
    "static freetype for the Qt wrap" FORCE)
set(JPEG_LIBRARY "/usr/x86_64-w64-mingw32/lib/libjpeg.a" CACHE FILEPATH
    "static libjpeg for the Qt wrap" FORCE)
set(JPEG_LIBRARY_RELEASE "/usr/x86_64-w64-mingw32/lib/libjpeg.a" CACHE FILEPATH
    "static libjpeg for the Qt wrap" FORCE)
set(PNG_LIBRARY "/usr/x86_64-w64-mingw32/lib/libpng16.a" CACHE FILEPATH
    "static libpng for the Qt wrap" FORCE)
set(PNG_LIBRARY_RELEASE "/usr/x86_64-w64-mingw32/lib/libpng16.a" CACHE FILEPATH
    "static libpng for the Qt wrap" FORCE)
set(ZLIB_LIBRARY "/usr/x86_64-w64-mingw32/lib/libz.a" CACHE FILEPATH
    "static zlib for the Qt wrap" FORCE)
set(ZLIB_LIBRARY_RELEASE "/usr/x86_64-w64-mingw32/lib/libz.a" CACHE FILEPATH
    "static zlib for the Qt wrap" FORCE)
set(PCRE2_LIBRARY_RELEASE "/usr/x86_64-w64-mingw32/lib/libpcre2-16.a" CACHE FILEPATH
    "static pcre2-16 for the Qt wrap" FORCE)
set(PCRE2_LIBRARY_DEBUG "/usr/x86_64-w64-mingw32/lib/libpcre2-16.a" CACHE FILEPATH
    "static pcre2-16 for the Qt wrap (debug too)" FORCE)
set(PCRE2_LIBRARIES "/usr/x86_64-w64-mingw32/lib/libpcre2-16.a" CACHE FILEPATH
    "static pcre2-16 for the Qt wrap" FORCE)
set(PCRE2_INCLUDE_DIRS "/usr/x86_64-w64-mingw32/include" CACHE PATH
    "pcre2 include dir for the Qt wrap" FORCE)
set(ZSTD_LIBRARY_RELEASE "/usr/x86_64-w64-mingw32/lib/libzstd.a" CACHE FILEPATH
    "static zstd for the Qt wrap" FORCE)
set(ZSTD_LIBRARIES "/usr/x86_64-w64-mingw32/lib/libzstd.a" CACHE FILEPATH
    "static zstd for the Qt wrap" FORCE)
set(BrotliDec_LIBRARY_RELEASE "/usr/x86_64-w64-mingw32/lib/libbrotlidec.a" CACHE FILEPATH
    "static brotli for the Qt wrap" FORCE)
set(BrotliEnc_LIBRARY_RELEASE "/usr/x86_64-w64-mingw32/lib/libbrotlienc.a" CACHE FILEPATH
    "static brotli for the Qt wrap" FORCE)
set(BrotliCommon_LIBRARY_RELEASE "/usr/x86_64-w64-mingw32/lib/libbrotlicommon.a" CACHE FILEPATH
    "static brotli for the Qt wrap" FORCE)
set(BrotliDec_LIBRARY_DEBUG "/usr/x86_64-w64-mingw32/lib/libbrotlidec.a" CACHE FILEPATH
    "static brotli for the Qt wrap" FORCE)
set(BrotliEnc_LIBRARY_DEBUG "/usr/x86_64-w64-mingw32/lib/libbrotlienc.a" CACHE FILEPATH
    "static brotli for the Qt wrap" FORCE)
set(BrotliCommon_LIBRARY_DEBUG "/usr/x86_64-w64-mingw32/lib/libbrotlicommon.a" CACHE FILEPATH
    "static brotli for the Qt wrap" FORCE)
set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "link static OpenSSL for the Qt wrap" FORCE)

