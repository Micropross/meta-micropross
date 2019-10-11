SUMMARY = "BSP files for Txgrl board."
DESCRIPTION = "Add BSP files for Txgrl board into rootfs."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI = " file://ec-txgrl-rev1.RW.bin \
          "
		  
FILES_${PN} = "/lib/firmware/ni/ec-txgrl-rev1.RW.bin \
					  "

S = "${WORKDIR}"

do_install() {
	install -d ${D}/lib/firmware/ni/
    install -D -m 0644 ${WORKDIR}/ec-txgrl-rev1.RW.bin ${D}/lib/firmware/ni/ec-txgrl-rev1.RW.bin
}