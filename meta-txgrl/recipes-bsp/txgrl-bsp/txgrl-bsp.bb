SUMMARY = "BSP files for Txgrl board."
DESCRIPTION = "Add BSP files for Txgrl board into rootfs."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI = " file://ec-txgrl-rev1.RW.bin \
			file://init_cts3.sh \
			file://txgrl-pl.dts \
			file://spi_nor.dts \
          "
		  
FILES_${PN} = "/lib/firmware/ni/ec-txgrl-rev1.RW.bin \
			   /usr/bin/init_cts3.sh \
			   /lib/firmware/txgrl-pl.dtbo \
			   /lib/firmware/spi_nor.dtbo \
			  "

S = "${WORKDIR}"

DEPENDS_append = " dtc-native"

do_compile() {
	dtc -@ -o ${WORKDIR}/txgrl-pl.dtbo ${WORKDIR}/txgrl-pl.dts
    dtc -@ -o ${WORKDIR}/spi_nor.dtbo ${WORKDIR}/spi_nor.dts
}

do_install() {
	install -d ${D}/lib/firmware/ni/
    install -D -m 0644 ${WORKDIR}/ec-txgrl-rev1.RW.bin ${D}/lib/firmware/ni/ec-txgrl-rev1.RW.bin
	
	install -d ${D}/usr/bin/
	install -D -m 0700 ${WORKDIR}/init_cts3.sh ${D}/usr/bin/init_cts3.sh
	
	install -D -m 0600 ${WORKDIR}/txgrl-pl.dtbo ${D}/lib/firmware/txgrl-pl.dtbo
	
	install -D -m 0600 ${WORKDIR}/spi_nor.dtbo ${D}/lib/firmware/spi_nor.dtbo
}