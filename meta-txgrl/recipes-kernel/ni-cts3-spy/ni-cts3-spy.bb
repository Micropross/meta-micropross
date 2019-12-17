SUMMARY = "Build the Linux kernel module for NI CTS3 SPY"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=5dcdfe25f21119aa5435eab9d0256af7"

inherit module

SRC_URI = "file://Makefile \
           file://ni_cts3_spy.c \
           file://COPYING \
          "

S = "${WORKDIR}"

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.

RPROVIDES_${PN} += "kernel-module-ni_cts3_spy"