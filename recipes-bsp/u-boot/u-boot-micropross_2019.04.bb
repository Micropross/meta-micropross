require recipes-bsp/u-boot/u-boot.inc

HOMEPAGE = "http://www.denx.de/wiki/U-Boot/WebHome"
SECTION = "bootloaders"

DEPENDS += "flex-native bison-native"
DEPENDS += "bc-native dtc-native"

LICENSE = "GPLv2+"
LIC_FILES_CHKSUM = "file://Licenses/README;md5=30503fd321432fc713238f582193b78e"
PE = "1"

S = "${WORKDIR}/git"

SRC_URI = "git://git@tau.bootlin.com/micropross/u-boot;protocol=ssh;branch=${SRCBRANCH}"

PV = "2019.04+git${SRCPV}"

SRCREV = "84d44c5f6e03d90b718f25ee1b98433bf769ea88"
SRCBRANCH = "v2019.04-micropross"

COMPATIBLE_MACHINE = "(imx6qdl-mp500)"
