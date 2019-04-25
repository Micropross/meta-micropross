SECTION = "kernel"
DESCRIPTION = "Linux kernel for Micropross mp500"
SUMMARY = "Linux kernel for Micropross mp500"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=bbea815ee2795b2f4230826c0c6b8814"

inherit kernel

FILESEXTRAPATHS_prepend := "${THISDIR}/${P}:"

S = "${WORKDIR}/git"

SRC_URI = "git://git@tau.bootlin.com/micropross/linux;protocol=ssh;branch=${SRCBRANCH}"
#SRC_URI += "file://defconfig"

PV = "4.19+${SRCPV}"

SRCREV = "${AUTOREV}"
SRCBRANCH = "v4.19.y-micropross"

COMPATIBLE_MACHINE = "(imx6qdl-mp500)"
