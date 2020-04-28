# Copyright (c) 2013-2014 LG Electronics, Inc.

SUMMARY = "Open Source Full Database Encryption for SQLite"
DESCRIPTION = "SQLCipher is an open source library that provides transparent, secure 256-bit AES encryption of SQLite database files."
LICENSE = "BSD"
DEPENDS = "tcl-native openssl"
LIC_FILES_CHKSUM = "file://LICENSE;md5=7edde5c030f9654613438a18b9b56308"

PR = "r1"
PV = "4.3.0"

inherit autotools

SRCREV = "ece66fdcbb6b43876d25e8e6308991a097fa8661"
SRC_URI = "git://github.com/sqlcipher/sqlcipher.git;protocol=git"

EXTRA_OECONF = "--disable-tcl CFLAGS=-DSQLITE_HAS_CODEC"
EXTRA_OEMAKE='"LIBTOOL=${STAGING_BINDIR_CROSS}/${HOST_SYS}-libtool"'

S = "${WORKDIR}/git"
