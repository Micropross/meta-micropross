DESCRIPTION = "Manufacturing flasher"
LICENSE = "CLOSED"

S="${WORKDIR}"

SRC_URI += "file://flasher.sh \
            file://inittab \
            file://passwd \
            file://shadow \
            file://group \
            file://init \
"

do_install () {
	install -d ${D}/proc/
	install -d ${D}/sys/
	install -d ${D}${sysconfdir}/
	install -d ${D}${sysconfdir}/init.d
	install -m 0755 ${WORKDIR}/inittab ${D}${sysconfdir}/inittab

	install -m 0755 ${WORKDIR}/passwd ${D}${sysconfdir}/passwd
	install -m 0755 ${WORKDIR}/shadow ${D}${sysconfdir}/shadow
	install -m 0755 ${WORKDIR}/group ${D}${sysconfdir}/group

	install -m 0755 ${WORKDIR}/init ${D}/init

	install -m 0755 ${WORKDIR}/flasher.sh ${D}${sysconfdir}/init.d/S50flasher.sh
}

FILES_${PN} += "/init /proc /sys"
