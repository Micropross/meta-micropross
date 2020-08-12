SUMMARY = "BSP files for Txgrl board."
DESCRIPTION = "Add BSP files for Txgrl board into rootfs."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI = " file://txgrl-pl.dts \
            file://spi-nor.dts \
            file://pcie.dts \
            file://interrupts.dts \
            file://ni_cts3_spy.dts \
          "

FILES_${PN} = "	/lib/firmware/txgrl-pl.dtbo \
                /lib/firmware/spi-nor.dtbo \
                /lib/firmware/pcie.dtbo \
                /lib/firmware/interrupts.dtbo \
                /lib/firmware/ni_cts3_spy.dtbo \
              "

S = "${WORKDIR}"

DEPENDS_append = " dtc-native"

do_compile() {
    dtc -@ -o ${WORKDIR}/txgrl-pl.dtbo ${WORKDIR}/txgrl-pl.dts
    dtc -@ -o ${WORKDIR}/spi-nor.dtbo ${WORKDIR}/spi-nor.dts
    dtc -@ -o ${WORKDIR}/pcie.dtbo ${WORKDIR}/pcie.dts
    dtc -@ -o ${WORKDIR}/interrupts.dtbo ${WORKDIR}/interrupts.dts
    dtc -@ -o ${WORKDIR}/ni_cts3_spy.dtbo ${WORKDIR}/ni_cts3_spy.dts
}

do_install() {
    install -d ${D}/lib/firmware
    install -D -m 0600 ${WORKDIR}/txgrl-pl.dtbo ${D}/lib/firmware/txgrl-pl.dtbo
    install -D -m 0600 ${WORKDIR}/spi-nor.dtbo ${D}/lib/firmware/spi-nor.dtbo
    install -D -m 0600 ${WORKDIR}/pcie.dtbo ${D}/lib/firmware/pcie.dtbo
    install -D -m 0600 ${WORKDIR}/interrupts.dtbo ${D}/lib/firmware/interrupts.dtbo
    install -D -m 0600 ${WORKDIR}/ni_cts3_spy.dtbo ${D}/lib/firmware/ni_cts3_spy.dtbo
}
