include u-boot_2018.07.inc

FILESEXTRAPATHS_prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://fw_env.config"

do_install_append () {
    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/fw_env.config ${D}${sysconfdir}/fw_env.config
}