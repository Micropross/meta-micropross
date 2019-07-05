require recipes-bsp/u-boot/u-boot.inc

HOMEPAGE = "http://www.denx.de/wiki/U-Boot/WebHome"
SECTION = "bootloaders"

DEPENDS += "flex-native bison-native"
DEPENDS += "bc-native dtc-native"

LICENSE = "GPLv2+"
LIC_FILES_CHKSUM = "file://Licenses/README;md5=30503fd321432fc713238f582193b78e"
PE = "1"

S = "${WORKDIR}/git"

SRC_URI = "git://git@github.com/Micropross/MP500_u-boot;protocol=ssh;branch=${SRCBRANCH}"

PV = "2019.04+git${SRCPV}"

SRCREV = "${AUTOREV}"
SRCBRANCH = "v2019.04-micropross"

do_compile_append () {
	cp ${B}/u-boot-dtb.imx ${B}/u-boot.imx
}


COMPATIBLE_MACHINE = "(imx6qdl-mp500)"
