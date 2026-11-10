#!/bin/sh
# pkg-config wrapper for the Windows static build: hides every .pc file so
# Qt's FindWrap* fall back to the pre-seeded static archives from the MinGW
# root instead of picking up the shared (.dll) variants from the sysroot.
PKG_CONFIG_PATH= PKG_CONFIG_LIBDIR=/nonexistent-pc-dir PKG_CONFIG_SYSROOT_DIR= PKG_CONFIG_SYSTEM_INCLUDE_PATH= PKG_CONFIG_SYSTEM_LIBRARY_PATH= exec /usr/sbin/pkg-config "$@"
